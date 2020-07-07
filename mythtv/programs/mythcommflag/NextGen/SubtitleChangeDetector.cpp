using namespace std;

// MythTV headers
#include "mythframe.h"
#include "mythplayer.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "SubtitleChangeDetector.h"

const int    kTeletextColumns = 40;
const int    kTeletextRows    = 26;

SubtitleChangeDetector::SubtitleChangeDetector(MythPlayer* player)
    : 
        m_teletextIsBlank(true),
        m_cc608IsBlank(true),
        m_cc708IsBlank(true)
{
    TeletextReader * teletextReader = player->GetTeletextReader();
    if (teletextReader)
    {
        int page = player->GetDecoder()->GetTrackLanguageIndex(
                            kTrackTypeTeletextCaptions,
                            player->GetDecoder()->GetTrack(kTrackTypeTeletextCaptions));
        teletextReader->SetPage(page, -1);
    }    
}

SubtitleChangeDetector::SubtitleTiming::SubtitleTiming(int start_, int end_)
    : start(start_), end(end_)
{
}

void SubtitleChangeDetector::Add(int start, int end)
{
    LOG(VB_COMMFLAG, LOG_DEBUG,
            QString("subtitle start %1 end %2")
            .arg(start).arg(end));

    if (!subtitleTimingList.empty())
    {
        MythDeque<SubtitleTiming>::iterator sit;
        for(sit = subtitleTimingList.begin(); sit != subtitleTimingList.end(); ++sit)
        {
            if (end < (*sit).start)
            {
                // wholy before current one so insert it
                subtitleTimingList.insert(sit, SubtitleTiming(start, end));
                break;
            }
            else if (start > (*sit).end)
            {
                // wholly after this one
            }
            else
            {
                if (start < (*sit).start)
                    (*sit).start = start;
                if (end > (*sit).end)
                    (*sit).end = end;
                break;
            }
        }
        if (sit == subtitleTimingList.end())
        {
            // wholly at the end
            subtitleTimingList.push_back(SubtitleTiming(start, end));
        }
    }
    else
    {
        subtitleTimingList.push_back(SubtitleTiming(start, end));
    }
}

void SubtitleChangeDetector::processFrame(MythPlayer* player, VideoFrame * frame)
{
#if 1
    SubtitleReader * reader = player->GetSubReader();
    int frameNumber = frame->frameNumber;
    double fps = player->GetFrameRate();
    //LOG(VB_COMMFLAG, LOG_DEBUG, QString("subtitle frame %1").arg(frameNumber));
#if 0
    uint64_t duration = 0;
    const QStringList rawSubs = reader->GetRawTextSubtitles(duration);

    if (!rawSubs.isEmpty())
    {
        LOG(VB_COMMFLAG, LOG_DEBUG,
                QString("There are also %1 raw text subtitles with duration %2")
                .arg(rawSubs.size()).arg(duration));
    }

    reader->ClearRawTextSubtitles();

#endif
    AVSubtitles *subtitles = reader->GetAVSubtitles();
    if (subtitles && !subtitles->buffers.empty())
    {
        QMutexLocker locker(&(subtitles->lock));
        while (!subtitles->buffers.empty())
        {
            const AVSubtitle subtitle = subtitles->buffers.front();
            subtitles->buffers.pop_front();

            //subtitle.start_display_time;    /* relative to packet pts, in ms */
            //subtitle.end_display_time;  /* relative to packet pts, in ms */
            //subtitle.pts;               /* in AV_TIME_BASE, use frame number * fps */

            int start = frameNumber + subtitle.start_display_time * fps / 1000;
            int end   = frameNumber + subtitle.end_display_time * fps / 1000;
            Add(start, end);
            reader->FreeAVSubtitle(subtitle);

            reader->ClearRawTextSubtitles();
        }
    }

    TeletextReader * teletextReader = player->GetTeletextReader();
    if (teletextReader)
    {
        if (teletextReader->PageChanged())
        {
            LOG(VB_COMMFLAG, LOG_DEBUG, QString("subtitle teletext changed 1"));
            TeletextSubPage *subpage = teletextReader->FindSubPage();
            if (subpage)
            {
                LOG(VB_COMMFLAG, LOG_DEBUG, QString("subtitle teletext subpage found %1 %2").arg((quintptr)subpage,0,16).arg(subpage->subpagenum));
                teletextReader->SetSubPage(subpage->subpagenum);
                int a = 0;
                if ((subpage->subtitle) ||
                        (subpage->flags & (TP_SUPPRESS_HEADER | TP_NEWSFLASH | TP_SUBTITLE)))
                {
                    LOG(VB_COMMFLAG, LOG_DEBUG, QString("subtitle teletext %1").arg(teletextReader->IsSubtitle()));
                    a = 1;
                    teletextReader->SetShowHeader(false);
                    teletextReader->SetIsSubtitle(true);
                    // check text is not blank
                    if (teletextReader->IsSubtitle() || teletextReader->IsTransparent())
                    {
                        QString caption;
                        bool isBlank = true;
                        unsigned char ch;
                        for (int y = kTeletextRows - a; y >= 2; y--)
                        {
                            for (uint i = (y == 1 ? 8 : 0); i < (uint) kTeletextColumns; i++)
                            {
                                ch = subpage->data[y-1][i] & 0x7F;
                                caption += ch;
                                if (ch != ' ')
                                {
                                    isBlank = false;
                                    //goto done_char_scan;
                                }
                            }
                        }
//done_char_scan:
                        m_teletextIsBlank = isBlank;
                        caption = caption.simplified();
                        LOG(VB_COMMFLAG, LOG_DEBUG, QString("new teletext subtitle string '%1' %2").arg(caption).arg(isBlank));
                    }
                    else
                    {
                        m_teletextIsBlank = true;
                    }

                }
                else
                {
                    teletextReader->SetShowHeader(true);
                    teletextReader->SetIsSubtitle(false);
                    teletextReader->SetHeaderChanged(false);
                    m_teletextIsBlank = true;
                }
            }
            teletextReader->SetPageChanged(false);
        }
        if (!m_teletextIsBlank)
        {
            //LOG(VB_COMMFLAG, LOG_DEBUG, QString("subtitle teletext present"));
            haveNewInformation(frameNumber, true, 0);
        }
    }
    CC608Reader * cc608Reader = player->GetCC608Reader();
    if (cc608Reader)
    {
        bool changed = false;
        CC608Buffer* textlist = cc608Reader->GetOutputText(changed);
        if (changed && textlist)
        {
            textlist->lock.lock();
            bool isBlank = true;
            QString caption;
            if (!textlist->buffers.empty())
            {
                for(vector<CC608Text*>::const_iterator it = textlist->buffers.begin();
                        it != textlist->buffers.end();
                        ++it)
                {
                    if (!(*it)->text.isEmpty())
                    {
                        isBlank = false;
                        caption += " " + (*it)->text;
                    }
                }
            }
            textlist->lock.unlock();
            m_cc608IsBlank = isBlank;
            caption = caption.simplified();
            LOG(VB_COMMFLAG, LOG_DEBUG, QString("new cc608 subtitle string '%1' %2").arg(caption).arg(isBlank));
        }
        if (!m_cc608IsBlank)
        {
            //LOG(VB_COMMFLAG, LOG_DEBUG, QString("subtitle cc608 present"));
            haveNewInformation(frameNumber, true, 0);
        }
    }
    CC708Reader * cc708Reader = player->GetCC708Reader();
    if (cc708Reader)
    {
        int cc708ChangedMask = 0;
        bool isBlank = true;
        QString caption;
        CC708Service *cc708service = cc708Reader->GetCurrentService();
        for (uint i = 0; i < 8; i++)
        {
            CC708Window &win = cc708service->windows[i];
            if (win.GetExists() && win.GetVisible() && !win.GetChanged())
                continue;
            if (!win.GetExists() || !win.GetVisible())
                continue;

            QMutexLocker locker(&win.lock);
            cc708ChangedMask |= 1<<i;
            vector<CC708String*> list = win.GetStrings();
            if (!list.empty())
            {
                for(vector<CC708String*>::const_iterator it = list.begin();
                        it != list.end();
                        ++it)
                {
                    if (!(*it)->str.isEmpty())
                    {
                        isBlank = false;
                        caption += " " + (*it)->str;
                    }
                }
            }
            for (uint j = 0; j < list.size(); j++)
                delete list[j];
            win.ResetChanged();
        }
        if (cc708ChangedMask != 0)
        {
            m_cc708IsBlank = isBlank;
            caption = caption.simplified();
            LOG(VB_COMMFLAG, LOG_DEBUG, QString("new cc708 subtitle string '%1' %2").arg(caption).arg(isBlank).arg(cc708ChangedMask,2,16,QChar('0')));
        }
        if (!m_cc708IsBlank)
        {
            //LOG(VB_COMMFLAG, LOG_DEBUG, QString("subtitle cc708 present"));
            haveNewInformation(frameNumber, true, 0);
        }
    }

    if (!subtitleTimingList.empty())
    {
        SubtitleTiming& currentSub = subtitleTimingList.front();
        if (currentSub.start <= frameNumber && frameNumber <= currentSub.end)
        {
            currentSub.start = frameNumber + 1;
            haveNewInformation(frameNumber, true, 0);
        }
        if (frameNumber >= currentSub.end)
        {
            subtitleTimingList.pop_front();
        }
    }
#endif
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
