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
    
    //verboseMask = VB_STDIO|VB_FLUSH|VB_GENERAL|VB_COMMFLAG;
    //logStart(QString(), 0, 0, -2, LOG_DEBUG, false, false, true);
	
	if (argc < 2)
	{
		std::cerr << "Usage: " << argv[0] << "[-nologo] [-noscene] frame.log" << std::endl;
		return 1;
	}

    bool logoDet = true, sceneDet = true;
    char const *filename = "/dev/null";

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            if (strcasecmp(argv[i], "-nologo") == 0)
                logoDet = false;
            else if (strcasecmp(argv[i], "-noscene") == 0)
                sceneDet = false;
        }
        else
        {
            filename = argv[i];
        }
    }
	
	std::ifstream ifs(filename);
	ifs.peek();
	if (!ifs)
	{
		std::cerr << "Failed to read " << filename << std::endl;
		return 1;
	}
	
	std::cerr << "Reading... " << std::endl;
	
	FrameMetadataAggregator aggregator;
	aggregator.configure(30, logoDet, sceneDet);
	
	uint64_t frameCount = 0;
	while (!!ifs)
	{
		int ch = ifs.peek();
		if (ch <= 0 || ch == '\r' || ch == '\n')
		{
			ifs.ignore(1); // and skip the newline
			continue;
		}
		
		if (ch == '#' || ch == 'N')
		{
			ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			continue;
		}
		
		if (ch == 'F')
		{
			ifs.ignore(4);
			double fps = 30;
			ifs >> fps;
			aggregator.configure(fps, logoDet, sceneDet);
			continue;
		}
		
		if (!isdigit(ch))
		{
			std::cerr << "Unexpected character: " << ch << "=" << (char)ch << std::endl;
			break;
		}
		
		FrameMetadata meta;
		ifs >> meta;
		if (!ifs)
			break;
		
		aggregator.add(meta);
		++frameCount;
		
		ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	}
	
	std::cerr << "Done reading, got " << frameCount << " frames" << std::endl;
	
	frm_dir_map_t commercialBreakList;
	aggregator.calculateBreakList(commercialBreakList);
	
	std::cout << "\n\n\n----------------------------" << endl;
	int count = 0;
    if (commercialBreakList.empty())
    {
		std::cout << "No breaks" << endl;
    }
    else
    {
        frm_dir_map_t::const_iterator it;
        for (it = commercialBreakList.begin(); 
			it != commercialBreakList.end();
			++it)
        {
            std::cout << "framenum: " << it.key() << "\tmarktype: " << *it << std::endl;
            if (*it == MARK_COMM_START)
				++count;
        }
    }
	std::cout << "----------------------------" << std::endl;
	std::cout << count << std::endl;
	
	return 0;
}
