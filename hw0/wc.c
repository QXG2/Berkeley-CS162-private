#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	FILE* fp;
	char ch;
	enum states {inWord, whiteSpace};
	int state = whiteSpace;
	int count_c=0, count_w=0, count_l=0;

	if (argc != 2) {
		printf("Only take one file! \n");
		exit(1); // syscall to terminate process, with error code 1
	}
	fp = fopen(argv[1],"r");
	if (fp == NULL) {
		printf("Fail to open this file! \n");
		exit(2);
	}
	
	// don't use feof(): is bad because it considers the last char twice.
	// use fscanf()==1, fgets()!=NULL or fgetc()!=EOF instead
	while (ch = fgetc(fp) != EOF) { //fgetc() get char from stream, and advance the stream 
		count_c ++;
		printf("%c", ch);
		if (state == whiteSpace) {
			if (ch == '\n') {
				count_l ++;
			} else if (!isspace(ch)) {  // ispace detect ' ','\n','\t','\v','\f','\r'
				state = inWord;
				count_w ++;
			}

		} else {  // state = inWord
			if (isspace(ch)) {
				state = whiteSpace;
				if (ch == '\n') {
					count_l ++;
				}
			}
		}
	}

	fclose(fp);

	printf("%d %d %d %s\n", count_l, count_w, count_c, argv[1]);
	return 0;
}
