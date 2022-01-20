#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sdl_ilbm/set.h>
#include <sdl_ilbm/image.h>
#include <jansson.h>

#define ILBM2FRAMES_VERSION "1.0.0"

static void usage(void)
{
	fprintf(stderr,
			"ilbm2frames %s\n"
			"\n"
			"Usage: ilbm2frames <input.iff> <outputDir>\n"
            "      --fps <n>           Number of frames <n> per second (Default: 30)\n"
            "      --limitSeconds <n>  Stop the animation after <n> seconds (Default: 5)\n"
			"  -h, --help              Output this help and exit\n"
			"  -V, --version           Output version and exit\n"
			"\n", ILBM2FRAMES_VERSION);
	exit(EXIT_FAILURE);
}

char * inputFilePath=0;
char * outputDirPath=0;
uint32_t fps=10;
uint32_t maxSeconds=3;

char * strchrtrim(char * str, char letter)
{
    if(!str)
        return str;

    while(*str==letter && *str)
	{
        strcpy(str, str+1);
	}

	return str;
}

char * strrchrtrim(char * str, char letter)
{
    if(!str)
       	return str;

    while(str[(strlen(str)-1)] == letter && strlen(str))
	{
        str[(strlen(str)-1)] = '\0';
	}

    return str;
}


static void parse_options(int argc, char **argv)
{
	int i;

	for(i=1;i<argc;i++)
	{
		int lastarg = i==argc-1;

		if(!strcmp(argv[i],"-h") || !strcmp(argv[i], "--help"))
		{
			usage();
		}
		else if(!strcmp(argv[i],"-V") || !strcmp(argv[i], "--version"))
		{
			printf("ilbm2frames %s\n", ILBM2FRAMES_VERSION);
			exit(EXIT_SUCCESS);
		}
        else if(!strcmp(argv[i], "--fps") && !lastarg)
        {
            fps = atol(argv[++i]);
        }
        else if(!strcmp(argv[i], "--limitSeconds") && !lastarg)
        {
            maxSeconds = atol(argv[++i]);
        }
		else
		{
			break;
		}
	}

	argc -= i;
	argv += i;

	if(argc<2)
		usage();

	inputFilePath = argv[0];
	outputDirPath = argv[1];
}

void ilbm2frames(json_t * imageJSON, SDL_ILBM_Set * set, unsigned int num)
{
	struct timeval tv;
	uint32_t msPerFrame = 1000/fps;
	uint32_t maxFrames = (fps*maxSeconds);
	SDL_Surface ** surfaces;
	surfaces = (SDL_Surface **)malloc(sizeof(SDL_Surface *)*maxFrames);

	SDL_ILBM_Image * image = SDL_ILBM_createImageFromSet(set, num, 0, SDL_ILBM_AUTO_FORMAT);
	if(image==NULL)
	{
	    fprintf(stderr, "Cannot open image #%d\n", num);
		return;
	}

	if(image->image->colorRangeLength==0 && image->image->cycleInfoLength==0 && image->image->drangeLength==0)
	{
		SDL_ILBM_freeImage(image);
		return;
	}

	json_t * colorRanges=json_array();
	for(uint32_t i=0;i<image->image->colorRangeLength;i++)
	{
		json_t * colorRange = json_object();
		json_object_set(colorRange, "pad1", json_integer(image->image->colorRange[i]->pad1));
		json_object_set(colorRange, "rate", json_integer(image->image->colorRange[i]->rate));
		json_object_set(colorRange, "active", json_integer(image->image->colorRange[i]->active));
		json_object_set(colorRange, "low", json_integer(image->image->colorRange[i]->low));
		json_object_set(colorRange, "high", json_integer(image->image->colorRange[i]->high));
		json_array_append(colorRanges, colorRange);
	}
	json_object_set(imageJSON, "colorRanges", colorRanges);

	json_t * cycleInfos=json_array();
	for(uint32_t i=0;i<image->image->cycleInfoLength;i++)
	{
		json_t * cycleInfo = json_object();
		json_object_set(cycleInfo, "pad", json_integer(image->image->cycleInfo[i]->pad));
		json_object_set(cycleInfo, "pad1", json_integer(image->image->cycleInfo[i]->direction));
		json_object_set(cycleInfo, "start", json_integer(image->image->cycleInfo[i]->start));
		json_object_set(cycleInfo, "end", json_integer(image->image->cycleInfo[i]->end));
		json_object_set(cycleInfo, "seconds", json_integer(image->image->cycleInfo[i]->seconds));
		json_object_set(cycleInfo, "microSeconds", json_integer(image->image->cycleInfo[i]->microSeconds));
		json_array_append(cycleInfos, cycleInfo);
	}
	json_object_set(imageJSON, "cycleInfos", cycleInfos);

	json_t * dRanges=json_array();
	for(uint32_t i=0;i<image->image->drangeLength;i++)
	{
		json_t * dRange = json_object();
		json_object_set(dRange, "min", json_integer(image->image->drange[i]->min));
		json_object_set(dRange, "max", json_integer(image->image->drange[i]->max));
		json_object_set(dRange, "rate", json_integer(image->image->drange[i]->rate));
		json_object_set(dRange, "flags", json_integer(image->image->drange[i]->flags));
		json_object_set(dRange, "ntrue", json_integer(image->image->drange[i]->ntrue));
		json_object_set(dRange, "nregs", json_integer(image->image->drange[i]->nregs));
		json_object_set(dRange, "nfades", json_integer(image->image->drange[i]->nfades));
		json_object_set(dRange, "pad", json_integer(image->image->drange[i]->pad));
		json_array_append(dRanges, dRange);
	}
	json_object_set(imageJSON, "dRanges", dRanges);

	for(uint32_t i=0;i<maxFrames;i++)
		surfaces[i] = SDL_ILBM_createSurface(image->image, 0, SDL_ILBM_AUTO_FORMAT);

	gettimeofday(&tv, 0);

	time_t startSec = tv.tv_sec;
	time_t startTick = (tv.tv_usec/1000);
	time_t nextFrame = startTick;
	uint32_t frameNum=0;
	char frameFilePath[2048];

	while(true)
	{
		gettimeofday(&tv, 0);
		
		time_t curTick = ((tv.tv_sec-startSec)*1000)+(tv.tv_usec/1000);
		if(curTick<nextFrame)
			continue;

		nextFrame+=msPerFrame;

		SDL_ILBM_cycleColors(image);
		SDL_ILBM_blitImageToSurface(image, NULL, surfaces[frameNum++], NULL);

		if(frameNum>=maxFrames)
			break;
	}

	for(uint32_t i=0;i<maxFrames;i++)
	{
		sprintf(frameFilePath, "%s/%02d_%05d.bmp", outputDirPath, num, i);
		SDL_SaveBMP(surfaces[i], frameFilePath);
		SDL_FreeSurface(surfaces[i]);
	}

	free(surfaces);

	SDL_ILBM_freeImage(image);

	json_object_set(imageJSON, "frameCount", json_integer(frameNum));
}

int main(int argc, char ** argv)
{
	json_t * json = json_object();

	parse_options(argc, argv);

    SDL_ILBM_Set * set = SDL_ILBM_createSetFromFilename(inputFilePath);
    if(set==NULL)
    {
        fprintf(stderr, "Error parsing ILBM file!\n");
        return 1;
    }

	if(SDL_Init(SDL_INIT_VIDEO)==-1)
    {
        fprintf(stderr, "Error initialising SDL video subsystem, reason: %s\n", SDL_GetError());
        SDL_ILBM_freeSet(set);
        return 1;
    }

	json_object_set(json, "imageCount", json_integer(set->imagesLength));

	json_t * imagesJSON=json_array();
	for(unsigned int num=0;num<set->imagesLength;num++)
	{
		json_t * imageJSON = json_object();
		ilbm2frames(imageJSON, set, num);
		json_array_append(imagesJSON, imageJSON);
	}
	json_object_set(json, "images", imagesJSON);

	printf("%s\n", json_dumps(json, 0));
	fflush(stdout);

    SDL_ILBM_freeSet(set);
	SDL_Quit();
}
