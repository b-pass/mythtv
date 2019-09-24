#ifndef VIDEO_DISPLAY_PROFILE_H_
#define VIDEO_DISPLAY_PROFILE_H_

// Qt
#include <QStringList>
#include <QMutex>
#include <QSize>
#include <QMap>

// MythTV
#include "mythtvexp.h"
#include "mythcontext.h"

// Std
#include <vector>
using namespace std;

struct RenderOptions
{
    QStringList               *renderers;
    QMap<QString,QStringList> *safe_renderers;
    QMap<QString,QStringList> *deints;
    QMap<QString,QStringList> *render_group;
    QMap<QString,uint>        *priorities;
    QStringList               *decoders;
    QMap<QString,QStringList> *equiv_decoders;
};

class MTV_PUBLIC ProfileItem
{
  public:
    ProfileItem() = default;
   ~ProfileItem() = default;

    // Sets
    void    Clear(void);
    void    SetProfileID(uint Id);
    void    Set(const QString &Value, const QString &Data);

    // Gets
    uint    GetProfileID(void) const;
    QString Get(const QString &Value) const;
    uint    GetPriority(void) const;
    QMap<QString,QString> GetAll(void) const;

    // Other
    bool CheckRange(QString Key, float Value, bool *Ok = nullptr) const;
    bool CheckRange(QString Key, int Value, bool *Ok = nullptr) const;
    bool CheckRange(const QString& Key, float FValue, int IValue, bool IsFloat, bool *Ok = nullptr) const;
    bool IsMatch(const QSize &Size, float Framerate, const QString &CodecName) const;
    bool IsValid(QString *Reason = nullptr) const;
    bool operator<(const ProfileItem &Other) const;
    QString toString(void) const;

  private:
    uint       m_profileid        { 0 };
    QMap<QString,QString> m_pref  { };
};

class MTV_PUBLIC VideoDisplayProfile
{
  public:
    VideoDisplayProfile();
   ~VideoDisplayProfile() = default;

    void    SetInput(const QSize &Size, float Framerate = 0, const QString &CodecName = QString());
    void    SetOutput(float Framerate);
    float   GetOutput(void) const;
    void    SetVideoRenderer(const QString &VideoRenderer);
    bool    CheckVideoRendererGroup(const QString &Renderer);
    QString GetDecoder(void) const;
    bool    IsDecoderCompatible(const QString &Decoder);
    uint    GetMaxCPUs(void) const ;
    bool    IsSkipLoopEnabled(void) const;
    QString GetVideoRenderer(void) const;
    QString GetDeinterlacer(void) const;
    QString GetFallbackDeinterlacer(void) const;
    QString GetActualVideoRenderer(void) const;
    QString GetFilters(void) const;
    QString GetFilteredDeint(const QString &Override);
    QString toString(void) const;

    // Statics
    static void        InitStatics(bool Reinit = false);
    static QStringList GetDecoders(void);
    static QStringList GetDecoderNames(void);
    static QString     GetDecoderName(const QString &Decoder);
    static QString     GetDecoderHelp(const QString &Decoder = QString());
    static QString     GetDefaultProfileName(const QString &HostName);
    static void        SetDefaultProfileName(const QString &ProfileName, const QString &HostName);
    static uint        GetProfileGroupID(const QString &ProfileName, const QString &HostName);
    static QStringList GetProfiles(const QString &HostName);
    static bool        DeleteProfileGroup(const QString &GroupName, const QString &HostName);
    static uint        CreateProfileGroup(const QString &ProfileName, const QString &HostName);
    static void        CreateProfile(uint GroupId, uint Priority,
                                     const QString& Compare1, uint Width1, uint Height1,
                                     const QString& Compare2, uint Width2, uint Height2,
                                     QString Decoder, uint MaxCpus, bool SkipLoop, QString VideoRenderer,
                                     QString Deint1, QString Deint2, QString Filters);
    static void        CreateProfile(uint GroupId, uint Priority,
                                     const QString& Width, const QString& Height, const QString& Codecs,
                                     const QString& Decoder, uint MaxCpus, bool SkipLoop, const QString& VideoRenderer,
                                     const QString& Deint1, const QString& Deint2, const QString& Filters);
    static void        DeleteProfiles(const QString &HostName);
    static void        CreateProfiles(const QString &HostName);
    static QStringList GetVideoRenderers(const QString &Decoder);
    static QString     GetVideoRendererHelp(const QString &Renderer);
    static QString     GetPreferredVideoRenderer(const QString &Decoder);
    static QStringList GetDeinterlacers(const QString &VideoRenderer);
    static QString     GetDeinterlacerName(const QString &ShortName);
    static QString     GetDeinterlacerHelp(const QString &Deinterlacer);
    static bool        IsFilterAllowed( const QString &VideoRenderer);
    static QStringList GetFilteredRenderers(const QString &Decoder, const QStringList &Renderers);
    static QString     GetBestVideoRenderer(const QStringList &Renderers);
    static vector<ProfileItem> LoadDB(uint GroupId);
    static bool        DeleteDB(uint GroupId, const vector<ProfileItem>& Items);
    static bool        SaveDB(uint GroupId, vector<ProfileItem>& Items);

  private:
    vector<ProfileItem>::const_iterator
            FindMatch(const QSize &Size, float Framerate, const QString &CodecName);
    void    LoadBestPreferences(const QSize &Size, float Framerate, const QString &CodecName);
    QString GetPreference(const QString &Key) const;
    void    SetPreference(const QString &Key, const QString &Value);

  private:
    mutable QMutex        m_lock                { QMutex::Recursive };
    QSize                 m_lastSize            { 0, 0 };
    float                 m_lastRate            { 0.0f };
    QString               m_lastCodecName       { };
    QString               m_lastVideoRenderer   { };
    QMap<QString,QString> m_currentPreferences  { };
    vector<ProfileItem>   m_allowedPreferences  { };

    static QMutex                    s_safe_lock;
    static bool                      s_safe_initialized;
    static QMap<QString,QStringList> s_safe_renderer;
    static QMap<QString,QStringList> s_safe_renderer_group;
    static QMap<QString,QStringList> s_safe_deint;
    static QMap<QString,QStringList> s_safe_equiv_dec;
    static QStringList               s_safe_custom;
    static QMap<QString,uint>        s_safe_renderer_priority;
    static QMap<QString,QString>     s_dec_name;
    static QStringList               s_safe_decoders;
};

#endif // VIDEO_DISPLAY_PROFILE_H_
