#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <limits>

#include <QCoreApplication>

#include "mythcontext.h"
#include "mythversion.h"
#include "mythlogging.h"

#include "FrameMetadataAggregator.h"

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
            ifs.open(argv[i]);
            ifs.peek();
            if (!ifs)
            {
                std::cerr << "Failed to open/read " << argv[i] << std::endl;
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
	
	std::cout << frameCount << std::endl;
	std::cout << fps << std::endl;
	std::cout << std::endl;
    aggregator.print(std::cout);
	
	return 0;
}
