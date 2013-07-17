//Jonathan Lehman
//April 2, 2011

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <pthread.h>
#include <math.h>

#define MAX_LINE 2048

struct buffer
{
	char **line;
	//mutex ensuring exclusive access to buffer
	pthread_mutex_t lock;
	//positions for reading and writing
	int readpos, writepos;
	//signaled when buffer is not empty
	pthread_cond_t notempty;
	//signaled when buffer is not full
	pthread_cond_t notfull;
};

struct fileList
{
	char* fname;
	struct stat info;
	struct fileList *next;
};

struct dataList
{
	char* entry;
	int num;
	struct dataList *next;
};

//mutex ensuring exclusive access to datalist
pthread_mutex_t lockDataList;

//file list
struct fileList *fileListHead = NULL;
struct fileList *fileListTail = NULL;
struct fileList *fileListNewNode, *fileListPtr;

//even list
struct dataList *evenListHead = NULL;
struct dataList *evenListTail = NULL;
struct dataList *evenListNewNode, *evenListPtr;

//odd list
struct dataList *oddListHead = NULL;
struct dataList *oddListTail = NULL;
struct dataList *oddListNewNode, *oddListPtr;

struct buffer buff;

//stores size of file list
int fileListSize = 0;
int evenListSize = 0;
int oddListSize = 0;

int numlines = 0;
int maxcounters = 0;
int filedelay = 0;
int threaddelay = 0;

int ctrNum = 1;

pthread_t th_counter[26];

//methods
void addFileToList(char*);
void init(struct buffer*);
void put(struct buffer*, char*);
char* get(struct buffer*);
void* reader(void*);
void* counter(void*);
void dealloc(struct buffer*);
void processLine(char*);
void addToEvenList(char*);
void addToOddList(char*);
char* getCtrName();
void usr1handler();

void usr1handler()
{
	//fprintf(stdout, "\nsignal raised %d\n", ctrNum);
	if(ctrNum < maxcounters)
	{
		ctrNum++;
		//create the thread
		pthread_create (&th_counter[ctrNum - 1], NULL, counter, (char*)getCtrName());
	}
}

int main(int argc, char *argv[])
{
	//attach handler to method to handle signals
	signal(SIGUSR1, usr1handler);
	int numLinesFound = 0;
	int maxCountersFound = 0;
	int fileDelayFound = 0;
	int threadDelayFound = 0;

	//keep track of which argument was last found
	int numLinesLast = 0;
	int maxCountersLast = 0;
	int fileDelayLast = 0;
	int threadDelayLast = 0;

	//check that there are at least 9 arguments not including file location
	if(argc < 10)
	{
		fprintf(stderr, "\ncounter: Invalid arguments for report. The correct invocation is:\ncounter -b numlines -t maxcounters -d filedelay -D threaddelay file ...\n");
		exit(1);
	}

	int k;
	for(k = 1; k < 9; k++)
	{

		if((k % 2) == 0)
		{

			char* invalChar;
			long arg;

			if(numLinesLast)
			{
				//check for overflow of argument
				if((arg = strtol(argv[k], &invalChar, 10)) >= INT_MAX)
				{
					fprintf(stderr, "\ncounter: Overflow. Invalid argument %d for counter, '%s'. The argument must be a valid non-zero positive integer less than %d.\n", k, argv[k], INT_MAX);
					exit(1);
				}

				//check that argument is a valid nonzero positive integer and check underflow
				if(!(arg > 1) || (*invalChar))
				{
					fprintf(stderr, "\ncounter: Invalid argument %d for counter, '%s'.  The argument must be a valid non-zero positive integer greater than one.\n", k, argv[k]);
					exit(1);
				}
				else
				{
					numlines = arg;
				}
			}
			else if(maxCountersLast)
			{
				//check for overflow of argument
				if((arg = strtol(argv[k], &invalChar, 10)) > 26)
				{
					fprintf(stderr, "\ncounter: Overflow. Invalid argument %d for counter, '%s'. The argument must be a valid non-zero positive integer less than %d.\n", k, argv[k], 26);
					exit(1);
				}

				//check that argument is a valid nonzero positive integer and check underflow
				if(!(arg > 0) || (*invalChar))
				{
					fprintf(stderr, "\ncounter: Invalid argument %d for counter, '%s'.  The argument must be a valid non-zero positive integer.\n", k, argv[k]);
					exit(1);
				}
				else
				{
					maxcounters = arg;
				}
			}
			else if(fileDelayLast)
			{
				//check for overflow of argument
				if((arg = strtol(argv[k], &invalChar, 10)) > INT_MAX)
				{
					fprintf(stderr, "\ncounter: Overflow. Invalid argument %d for counter, '%s'. The argument must be a valid non-zero positive integer less than %d.\n", k, argv[k], INT_MAX);
					exit(1);
				}

				//check that argument is a valid nonzero positive integer and check underflow
				if(!(arg > -1) || (*invalChar))
				{
					fprintf(stderr, "\ncounter: Invalid argument %d for counter, '%s'.  The argument must be a valid non-zero positive integer.\n", k, argv[k]);
					exit(1);
				}
				else
				{
					filedelay = arg;
				}
			}
			else if(threadDelayLast)
			{
				//check for overflow of argument
				if((arg = strtol(argv[k], &invalChar, 10)) > INT_MAX)
				{
					fprintf(stderr, "\ncounter: Overflow. Invalid argument %d for counter, '%s'. The argument must be a valid non-zero positive integer less than %d.\n", k, argv[k], INT_MAX);
					exit(1);
				}

				//check that argument is a valid nonzero positive integer and check underflow
				if(!(arg > -1) || (*invalChar))
				{
					fprintf(stderr, "\ncounter: Invalid argument %d for counter, '%s'.  The argument must be a valid non-zero positive integer.\n", k, argv[k]);
					exit(1);
				}
				else
				{
					threaddelay = arg;
				}
			}
			else
			{
				fprintf(stderr, "\ncounter: Invalid argument %d for counter, '%s'.\n", k - 1, argv[k - 1]);
				exit(1);
			}

			//reset last variable found
			numLinesLast = 0;
			maxCountersLast = 0;
			fileDelayLast = 0;
			threadDelayLast = 0;
		}
		else
		{
			if(strcmp(argv[k], "-b") == 0)
			{
				numLinesFound = 1;
				numLinesLast = 1;
			}
			else if(strcmp(argv[k], "-t") == 0)
			{
				maxCountersFound = 1;
				maxCountersLast = 1;
			}
			else if(strcmp(argv[k], "-d") == 0)
			{
				fileDelayFound = 1;
				fileDelayLast = 1;
			}
			else if(strcmp(argv[k], "-D") == 0)
			{
				threadDelayFound = 1;
				threadDelayLast = 1;
			}
			else//invalid arg
			{
				fprintf(stderr, "\ncounter: Invalid argument %d for counter, '%s'.\n", k, argv[k]);
				exit(1);
			}
		}
	}

	//check that all vars have been given values
	if(!(numLinesFound && maxCountersFound && fileDelayFound && threadDelayFound))
	{
		fprintf(stderr, "\ncounter: Invalid arguments for report. The correct invocation is:\ncounter -b numlines -t maxcounters -d filedelay -D threaddelay file ...\n");
		exit(1);
	}

	//test args
	//fprintf(stdout, "-b %d -t %d -d %d -D %d\n", numlines, maxcounters, filedelay, threaddelay);

	//add all file names to linked list, each file name no longer than 4095 char
	int i;
	for(i = 9; i < argc; i++)
	{
		addFileToList(argv[i]);
	}

	//initialize pthread mutexes and conditions for even and odd lists
	pthread_mutex_init (&lockDataList, NULL);

	pthread_t th_reader;
	void *retval;
	init(&buff);

	//create the threads
	pthread_create (&th_reader, NULL, reader, 0);
	pthread_create (&th_counter[ctrNum - 1], NULL, counter, (char*)getCtrName());

	//wait until reader and counters finish
	pthread_join (th_reader, &retval);
	int j;
	for(j = 0; j < ctrNum; j++)
	{
		pthread_join(th_counter[j], &retval);
	}

	//print even list
	fprintf(stdout, "\nEven List (%d entries):\n", evenListSize);
	for(evenListPtr = evenListHead; evenListPtr != NULL; evenListPtr = evenListPtr->next){
		fprintf(stdout, "Entry: %s Occurrences: %d\n", evenListPtr->entry, evenListPtr->num);
	}

	//print odd list
	fprintf(stdout, "\nOdd List (%d entries):\n", oddListSize);
	for(oddListPtr = oddListHead; oddListPtr != NULL; oddListPtr = oddListPtr->next){
		fprintf(stdout, "Entry: %s Occurrences: %d\n", oddListPtr->entry, oddListPtr->num);
	}

	dealloc(&buff);

	return 0;
}

void addFileToList(char* fileName)
{
	int fileLength = strlen(fileName) + 1;

	if((fileListNewNode = (struct fileList *)malloc(sizeof(struct fileList))) == NULL)
	{
		fprintf(stderr, "malloc failure for fileListNewNode\n");
		exit(1);
	}

	if((fileListNewNode->fname = malloc(fileLength)) == NULL)
	{
		fprintf(stderr, "malloc failure for fileListNewNode->fname\n");
		exit(1);
	}

	if(strncpy(fileListNewNode->fname, fileName, fileLength) != fileListNewNode->fname)
	{
		fprintf(stderr, "string copy problem\n");
		exit(1);
	}

	if (stat(fileName, &fileListNewNode->info) != 0) {
		fprintf(stderr,"counter: cannot use file, %s, because:\n", fileName);
		perror(NULL);
	}
	else
	{
		int fileUsed = 0;

		//check if file is a regular file (only add regular files to list)
		if(S_ISREG(fileListNewNode->info.st_mode)){
			//check if file reference is already part of list
				//iterate through list
			for(fileListPtr = fileListHead; fileListPtr != NULL; fileListPtr = fileListPtr->next){
				if((major(fileListPtr->info.st_dev) == major(fileListNewNode->info.st_dev)) && (minor(fileListPtr->info.st_dev) == minor(fileListNewNode->info.st_dev)) && (((long)fileListPtr->info.st_ino) == ((long)fileListNewNode->info.st_ino))){
					fileUsed = 1;
					fprintf(stderr,"counter: Duplicate file in arguments, %s.  Will only process one.\n", fileName);
				}
			}

			//add stat info for file to linked list if it has not already been added
			if(!fileUsed){
				fileListNewNode->next = NULL;

				//first insertion is a special case, otherwise append to end of list
				if(fileListTail == NULL)
				{
					fileListHead = fileListTail = fileListNewNode;
					fileListSize = fileListSize + 1;
				}
				else
				{
					fileListTail->next = fileListNewNode;
					fileListTail = fileListNewNode;
					fileListSize = fileListSize + 1;
				}
			}
		}
	}//end else
}

//deallocate buffer
void dealloc(struct buffer *b)
{
	int i;
	for(i = 0; i < numlines; i++)
	{
		free(b->line[i]);
	}

	free(b->line);
}

//initialize buffer
void init(struct buffer *b)
{
	b->line = (char **)malloc(numlines * sizeof(char *));
	int i;
	for(i = 0; i < numlines; i++)
		b->line[i] = (char *)malloc(MAX_LINE * sizeof(char));

	pthread_mutex_init (&b->lock, NULL);
	pthread_cond_init (&b->notempty, NULL);
	pthread_cond_init (&b->notfull, NULL);
	b->readpos = 0;
	b->writepos = 0;
}

//store a line in the buffer
void put(struct buffer *b, char* data)
{
	pthread_mutex_lock (&b->lock);

	int firstTimeThrough = 1;
	//wait until buffer is not full
	while((b->writepos + 1) % numlines == b->readpos)
	{
		if(firstTimeThrough)
		{
			raise(SIGUSR1);
			firstTimeThrough = 0;
		}
		pthread_cond_wait(&b->notfull, &b->lock);
		//pthread_cond_wait reacquired b->lock before returning
	}

	//write the data and advance write pointer
	strcpy(b->line[b->writepos], data);

	b->writepos++;
	if(b->writepos >= numlines)
	{
		b->writepos = 0;
	}

	//signal that the buffer is now not empty
	pthread_cond_signal(&b->notempty);
	pthread_mutex_unlock (&b->lock);
}

//read and remove a line from the buffer
char* get(struct buffer *b)
{
	pthread_mutex_lock(&b->lock);

	//wait until buffer is not empty
	while(b->writepos == b->readpos)
	{
		pthread_cond_wait(&b->notempty, &b->lock);
	}

	//read data and advance read pointer
	int oldReadPos = b->readpos;

	b->readpos++;
	if(b->readpos >= numlines)
	{
		b->readpos = 0;
	}

	//signal that buffer is now not full
	pthread_cond_signal (&b->notfull);
	pthread_mutex_unlock (&b->lock);

	return b->line[oldReadPos];
}

//code for reader thread
void *reader(void *data)
{
	FILE* file = NULL;
	struct timespec t, t2;
	int seconds = floor(filedelay / 1000);
	int milliseconds = filedelay - (seconds * 1000);
	t.tv_sec = seconds;
	t.tv_nsec = milliseconds * 1000000;

	//iterate through files
	for(fileListPtr = fileListHead; fileListPtr != NULL; fileListPtr = fileListPtr->next)
	{

		if((file = fopen(fileListPtr->fname,"r")) == NULL)
		{
			fprintf(stderr, "\ncounter: The file, %s, could not be opened.\n", fileListPtr->fname);
		}
		else//file was opened
		{

			//read each line from file
			char readBuffer[MAX_LINE];
			while(fgets(readBuffer, MAX_LINE, file) != 0)
			{
				//put line in buff
				put(&buff, readBuffer);

				//sleep for filedelay milliseconds
				if(nanosleep(&t, &t2) < 0)
				{
				  fprintf(stderr, "\ncounter: nanosleep for file delay %d milliseconds failed.\n", filedelay);
				}
			}

			//close the file when done with it
			if(fclose(file) != 0)
			{
				fprintf(stderr, "\ncounter: The file, %s, was not closed properly.\n");
				file = NULL;
			}
		}
	}

	//put line in buff to signal of reader
	int i;
	for(i = 0; i < ctrNum; i++)
	{
		put(&buff, "-1");
	}

	return NULL;
}

//code for counter thread
void *counter(void *data)
{
	char *d = malloc(sizeof(char) * MAX_LINE);

	struct timespec t, t2;
	int seconds = floor(threaddelay / 1000);
	int milliseconds = threaddelay - (seconds * 1000);
	t.tv_sec = seconds;
	t.tv_nsec = milliseconds * 1000000;

	while(1)
	{
		//print ctr name while it is working
		fprintf(stdout, "%s", data);
		fflush(stdout);
		//get line from buffer
		strcpy(d, get(&buff));

		//check if done (reads -1 from buff)
		if(strcmp(d, "-1") == 0)
			break;

		//counter process line
		pthread_mutex_lock(&lockDataList);
		processLine(d);
		pthread_mutex_unlock(&lockDataList);

		memset(d, 0, MAX_LINE);

		//sleep threaddelay milliseconds before doing anything with data
		if(nanosleep(&t, &t2) < 0)
		{
		  fprintf(stderr, "\ncounter: nanosleep for threaddelay %d milliseconds failed.\n", threaddelay);
		}
	}

	free(d);
	return NULL;
}

void processLine(char *line)
{
	char* delimiters = "\t\n\v\f\r ";

	char* linePtr = strtok(line, delimiters);

	while(linePtr != NULL)
	{
		//check if even or odd
		if(strlen(linePtr) % 2 == 0)//even
		{
			addToEvenList(linePtr);
		}
		else//odd
		{
			addToOddList(linePtr);
		}
		//increment token
		linePtr = strtok(NULL, delimiters);
	}
}

void addToEvenList(char *data)
{
	strcat(data, "\0");

	int size = sizeof(char) * strlen(data) + 1;

	if((evenListNewNode = (struct dataList *)malloc(sizeof(struct dataList))) == NULL)
	{
		fprintf(stderr, "malloc failure for evenListNewNode\n");
		exit(1);
	}

	if((evenListNewNode->entry = malloc(size)) == NULL)
	{
		fprintf(stderr, "malloc failure for evenListNewNode->entry\n");
		exit(1);
	}

	if(strncpy(evenListNewNode->entry, data, size) != evenListNewNode->entry)
	{
		fprintf(stderr, "string copy problem\n");
		exit(1);
	}

	evenListNewNode->num = 1;

	int hasPrevious = 0;
	int entryAdded = 0;

	//put data in correct location or increment num value
	struct dataList *prevListPtr;

	for(evenListPtr = evenListHead; evenListPtr != NULL && entryAdded != 1; evenListPtr = evenListPtr->next)
	{

		if(strcmp(evenListNewNode->entry, evenListPtr->entry) == 0)
		{
			//increment num if equal
			evenListPtr->num++;
			entryAdded = 1;
			evenListSize++;
		}
		else if(strcmp(evenListNewNode->entry, evenListPtr->entry) < 0)
		{
			if(hasPrevious)//insert in middle
			{
				evenListNewNode->next = evenListPtr;
				prevListPtr->next = evenListNewNode;
			}
			else//insert at beginning of list (if less than first one)
			{
				evenListNewNode->next = evenListHead;
				evenListHead = evenListNewNode;
			}
			evenListSize++;
			entryAdded = 1;
		}
		else//strcmp > 0
		{
			prevListPtr = evenListPtr;
		}

		hasPrevious = 1;
	}

	//add entry if first entry
	if(evenListSize == 0)
	{
		evenListHead = evenListTail = evenListNewNode;
		evenListSize++;
		entryAdded = 1;
	}

	//add element to end of list if it is greater than the rest
	if(!entryAdded)
	{
		evenListTail->next = evenListNewNode;
		evenListTail = evenListNewNode;
		evenListSize++;
	}

	//prevents segfaults
	evenListTail->next = NULL;
}

void addToOddList(char *data)
{
	strcat(data, "\0");

	int size = sizeof(char) * strlen(data) + 1;

	if((oddListNewNode = (struct dataList *)malloc(sizeof(struct dataList))) == NULL)
	{
		fprintf(stderr, "malloc failure for oddListNewNode\n");
		exit(1);
	}

	if((oddListNewNode->entry = malloc(size)) == NULL)
	{
		fprintf(stderr, "malloc failure for oddListNewNode->entry\n");
		exit(1);
	}

	if(strncpy(oddListNewNode->entry, data, size) != oddListNewNode->entry)
	{
		fprintf(stderr, "string copy problem\n");
		exit(1);
	}

	oddListNewNode->num = 1;

	int hasPrevious = 0;
	int entryAdded = 0;

	//put data in correct location or increment num value
	struct dataList *prevListPtr;
	for(oddListPtr = oddListHead; oddListPtr != NULL && entryAdded != 1; oddListPtr = oddListPtr->next)
	{

		if(strcmp(oddListNewNode->entry, oddListPtr->entry) == 0)
		{
			//increment num if equal
			oddListPtr->num++;
			entryAdded = 1;
			oddListSize++;
		}
		else if(strcmp(oddListNewNode->entry, oddListPtr->entry) < 0)
		{
			if(hasPrevious)//insert in middle
			{
				oddListNewNode->next = oddListPtr;
				prevListPtr->next = oddListNewNode;
			}
			else//insert at beginning of list (if less than first one)
			{
				oddListNewNode->next = oddListHead;
				oddListHead = oddListNewNode;
			}
			oddListSize++;
			entryAdded = 1;
		}
		else//strcmp > 0
		{
			prevListPtr = oddListPtr;
		}

		hasPrevious = 1;
	}

	//add entry if first entry
	if(oddListSize == 0)
	{
		oddListHead = oddListTail = oddListNewNode;
		oddListSize++;
		entryAdded = 1;
	}

	//add element to end of list if it is greater than the rest
	if(!entryAdded)
	{
		oddListTail->next = oddListNewNode;
		oddListTail = oddListNewNode;
		oddListSize++;
	}

	//prevents segfaults
	oddListTail->next = NULL;
}

char* getCtrName()
{
	switch(ctrNum)
	{
		case 1: return "a";
	    case 2: return "b";
	    case 3: return "c";
	    case 4: return "d";
	    case 5: return "e";
		case 6: return "f";
		case 7: return "g";
		case 8: return "h";
		case 9: return "i";
		case 10: return "j";
		case 11: return "k";
		case 12: return "l";
		case 13: return "m";
		case 14: return "n";
		case 15: return "o";
		case 16: return "p";
		case 17: return "q";
		case 18: return "r";
		case 19: return "s";
		case 20: return "t";
		case 21: return "u";
		case 22: return "v";
		case 23: return "w";
		case 24: return "x";
		case 25: return "y";
		case 26: return "z";
		default:
			return "!";
	}
}
