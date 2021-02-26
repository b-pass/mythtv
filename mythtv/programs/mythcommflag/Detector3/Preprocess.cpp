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
    
	if (argc < 2)
	{
		std::cerr << "Usage: " << argv[0] << " [-nologo] [-noscene] <frame.log|-->" << std::endl;
		return 1;
	}

    double fps  = -1;
    bool logoDet = true, sceneDet = true, audioDet = true;
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
            else if (strcasecmp(argv[i], "--") == 0)
                is = &std::cin;
            else if (strcasecmp(argv[i], "-f") == 0)
                fps = atof(argv[++i]);
        }
        else
        {
            //ifs.close(); ifs.clear();
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
    
	FrameMetadataAggregator aggregator;
	aggregator.configure(30, logoDet, sceneDet, audioDet);
	
	uint64_t frameCount = 0;
    std::string line;
	while (std::getline(*is,line))
	{
		if (line.empty() || line[0] == '\r' || line[0] == '\n')
			continue;
		
		if (line[0] == '#' || line[0] == 'N' || line[0] == 'f')
			continue;
		
		if (line[0] == 'F')
		{
            std::istringstream iss(line.substr(4));
            double tmpFps = 0;
			iss >> tmpFps;
            if (fps < 1 && tmpFps > 0)
                fps = tmpFps;
			aggregator.configure(fps, logoDet, sceneDet, audioDet);
			continue;
		}
        
        if (line.size() < 15)
            continue;
        
		if (!isdigit(line[0]))
		{
			std::cerr << "Unexpected character: " << (int)line[0] << "=" << (char)line[0] << std::endl;
			return 2;
		}
		
		FrameMetadata meta;
        
        std::istringstream iss(line);
        iss >> meta;
        
		aggregator.add(meta);
		++frameCount;
	}
	
    std::cout << "# " << filename << std::endl;
    if (frameCount > 0)
        aggregator.nnPrint(std::cout);

    unlink("/tmp/mcfunxz");
	return 0;
}
