#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <limits>
#include <unistd.h>
#include <glob.h>

#include <QCoreApplication>

#include "mythcontext.h"
#include "mythversion.h"
#include "mythlogging.h"

#include "FrameMetadataAggregator.h"

std::string findLog(char const *);
std::string findLog(char const *file)
{
    char const *temp = realpath(file, NULL);
    if (!temp)
        temp = file;

    std::string filename = temp;
    size_t p = filename.rfind('.');
    if (p == std::string::npos)
        return filename;
    
    if (filename.compare(p, 4, ".log", 4) == 0 && p+4 == filename.length())
        return filename;
        
    if (filename.compare(p, 4, ".mpg", 4) == 0 && p+4 == filename.length())
        filename.replace(p, 4, ".log*");
    else if (filename.compare(p, 4, ".txt", 4) == 0 && p+4 == filename.length())
        filename.replace(p, 4, ".log*");
    else if (filename.compare(p, 3, ".ts", 3) == 0 && p+3 == filename.length())
        filename.replace(p, 3, ".log*");            
    p = filename.rfind('/');
    if (p != std::string::npos)
        filename.insert(p+1, "../*/*");
    //filename.replace(filename.size() - 9, 4, "*");
    
    glob_t g;
    memset(&g, 0, sizeof(g));
    glob(filename.c_str(), 0, NULL, &g);
    if (g.gl_pathc)
    {
        filename = g.gl_pathv[0];
        if (filename.compare(filename.size() - 3, 3, ".xz") == 0)
        {
            p = filename.rfind('_');
            std::string chanid, starttime;
            if (p != std::string::npos && p != 0)
            {
				size_t cp = p - 1;
				while (cp >= 0 && isdigit(filename[cp]))
					--cp;
				++cp;
				chanid = filename.substr(cp, p - cp);
				size_t sp = p + 1;
				while (sp < filename.size() && isdigit(filename[sp]))
					starttime += filename[sp++];
			}
			std::string cmd = "xzcat " + filename;
			if (!chanid.empty() && !starttime.empty())
				cmd += " | ./fixtimes.py " + chanid + " " + starttime;
			cmd += " > /tmp/mcfunxz";
            std::cerr << cmd << std::endl;
            int res = system(cmd.c_str());
            if (res == 0)
                filename = "/tmp/mcfunxz";
            else
                std::cerr << "xzcat failed!" << std::endl;
        }
        return filename;
    }
    else
    {
        return file;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init( false, /*use gui*/
                         false, /*prompt for backend*/
                         false, /*bypass auto discovery*/
                         true ))/*ignoreDB*/
	{
		return 1;
	}
    
    //verboseMask = VB_STDIO|VB_FLUSH|VB_GENERAL|VB_COMMFLAG;
    //logStart(QString(), 0, 0, -2, LOG_DEBUG, false, false, true);
	
	if (argc < 2)
	{
		std::cerr << "Usage: " << argv[0] << " [-nologo] [-noscene] [-noaudio] [-debug] [-nn] <frame.log|-->" << std::endl;
		return 1;
	}

    bool verbose = false;
    bool logoDet = true, sceneDet = true, audioDet = true;
    bool nn = true;
    std::istream *is = nullptr;
    std::ifstream ifs;
    std::string filename;
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            if (strcasecmp(argv[i], "-nologo") == 0)
                logoDet = false;
            else if (strcasecmp(argv[i], "-noscene") == 0)
                sceneDet = false;
            else if (strcasecmp(argv[i], "-noaudio") == 0)
                audioDet = false;
            else if (strcasecmp(argv[i], "-debug") == 0)
                FrameMetadataAggregator::scoreDebugging = true;
            else if (strcasecmp(argv[i], "-verbose") == 0 || strcasecmp(argv[i], "-v") == 0)
                verbose = true;
            else if (strcasecmp(argv[i], "-nn") == 0)
                nn = true;
            else if (strcasecmp(argv[i], "-nonn") == 0)
                nn = false;
            else if (strcasecmp(argv[i], "--") == 0)
                is = &std::cin;
        }
        else
        {
            ifs.close();
            ifs.clear();
            filename = findLog(argv[i]);
            std::cerr << "Opening " << filename << std::endl;
            ifs.open(filename.c_str());
            ifs.peek();
            if (!ifs)
            {
                std::cerr << "Failed to open/read " << filename << std::endl;
                return 1;
            }
            is = &ifs;
        }
    }
	if (verbose)
		std::cerr << "Reading... " << std::endl;
	
	FrameMetadataAggregator aggregator;
	aggregator.configure(30, logoDet, sceneDet, audioDet);
	
	double fps = 0;
	uint64_t frameCount = 0;
	while (!!*is)
	{
		int ch = is->peek();
		if (ch <= 0 || ch == '\r' || ch == '\n')
		{
			is->ignore(1); // and skip the newline
			continue;
		}
		
		if (ch == '#' || ch == 'N')
		{
			is->ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			continue;
		}
		
		if (ch == 'F')
		{
			is->ignore(4);
			*is >> fps;
			aggregator.configure(fps, logoDet, sceneDet, audioDet);
			continue;
		}
		
		if (!isdigit(ch))
		{
			std::cerr << "Unexpected character: " << ch << "=" << (char)ch << std::endl;
			break;
		}
		
		FrameMetadata meta;
		*is >> meta;
		if (!*is)
			break;
		
		aggregator.add(meta);
		++frameCount;
		
		is->ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	}
	
	if (verbose)
		std::cerr << "Done reading, got " << frameCount << " frames" << std::endl;
	std::cerr << frameCount << std::endl;
	std::cerr << fps << std::endl;
	std::cerr << std::endl;
	
	frm_dir_map_t commercialBreakList;
    if (nn)
    {
        unlink("/tmp/mcfnn");
        {
            std::ofstream ofs("/tmp/mcfnn");
            aggregator.nnPrint(ofs);
            if (!ofs)
                std::cerr << "Error writing /tmp/mcfnn" << std::endl;
        }
        FILE *resf = popen("mythcommflagneural /tmp/mcfnn", "r");
        while (!feof(resf))
        {
            unsigned int fs = 0;
            int sc = 0;
            if (fscanf(resf, " %u , %d \n", &fs, &sc) == 2)
                aggregator.nnTweak(fs, sc);
            else
                fgetc(resf);
        }
        pclose(resf);
        unlink("/tmp/mcfnn");
    }
    
    aggregator.calculateBreakList(commercialBreakList, nn);
    if (nn) {
        std::cout << "# " << filename << std::endl;
        aggregator.nnPrint(std::cout, false);
    }
    
	std::cerr << "\n\n\n----------------------------" << std::endl;
	int count = 0;
	if (commercialBreakList.empty())
	{
		std::cerr << "No breaks" << std::endl;
	}
	else
	{
        	frm_dir_map_t::const_iterator it;
	        for (it = commercialBreakList.begin(); 
			it != commercialBreakList.end();
			++it)
		{
			std::cerr << "framenum: " << it.key() << "\tmarktype: " << *it << std::endl;
			if (*it == MARK_COMM_START)
				++count;
		}
	}
	
	std::cerr << "----------------------------" << std::endl;
	std::cerr << count << std::endl;
	
	return 0;
}
