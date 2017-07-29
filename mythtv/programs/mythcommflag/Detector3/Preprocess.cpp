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
    else if (filename.compare(p, 3, ".ts", 3) == 0 && p+3 == filename.length())
        filename.replace(p, 3, ".log*");            
    p = filename.rfind('/');
    if (p != std::string::npos)
        filename.insert(p+1, "../*/*");
    filename.replace(filename.size() - 9, 4, "*");
    
    glob_t g;
    memset(&g, 0, sizeof(g));
    glob(filename.c_str(), 0, NULL, &g);
    if (g.gl_pathc)
    {
        filename = g.gl_pathv[0];
        if (filename.compare(filename.size() - 3, 3, ".xz") == 0)
        {
            std::cerr << "xzcat " << filename << std::endl;
            int res = system(("xzcat " + filename + " > /tmp/mcfunxz 2>&1").c_str());
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
		std::cerr << "Usage: " << argv[0] << "[-nologo] [-noscene] <frame.log|-->" << std::endl;
		return 1;
	}

    bool logoDet = true, sceneDet = true, audioDet = true;
    std::istream *is = nullptr;
    std::ifstream ifs;

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
        }
        else
        {
            ifs.close();
            ifs.clear();
            std::string filename = findLog(argv[i]);
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
			return 2;
		}
		
		FrameMetadata meta;
		*is >> meta;
		if (!*is)
			break;
		
		aggregator.add(meta);
		++frameCount;
		
		is->ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	}
	
    aggregator.nnprint(std::cout);

    unlink("/tmp/mcfunxz");
	return 0;
}
