/* Generate a batch file that sorts the words from every wikipedia with double capital
* letter like AA, AB, AC, .. by the number of occurences */

#include <stdlib.h>
#include <stdio.h>

// Writes the wgets commands into the batch file
void save_wiki_pages(FILE *batch_file) {
	char i, j;

	// Need to get everypage from AA to ZZ
	for (i = 'A'; i <= 'Z'; ++i) {
		for (j = 'A'; j <= 'Z'; ++j) {
			if ((fprintf(batch_file, "wget https://en.wikipedia.org/wiki/%c%c -O %c%c.html\n", i, j, i, j)) < 0) {
				perror("fprintf");
				exit(EXIT_FAILURE);
			}	
		}
	}
}

// Puts statements in batch file that remove html tags 
// Commands creates a text file for every different web page
void extract_text(FILE *batch_file) {
	char i, j;

	// Need to get everypage from AA to ZZ
	for (i = 'A'; i <= 'Z'; ++i) {
		for (j = 'A'; j <= 'Z'; ++j) {
			if ((fprintf(batch_file, "lynx -dump -nolist %c%c.html > %c%c.txt\n", i, j, i, j)) < 0) {
				perror("fprintf");
				exit(EXIT_FAILURE);
			}	
		}
	}
}

// Puts grep command into batch file
// Single grep command for all files
void find_words(FILE *batch_file) {
	char i, j;

	// Put the beginning of the grep command into the batch file
	if ((fprintf(batch_file, "grep -oh \"[a-zA-Z]*\" ")) < 0) {
		perror("fprintf");
		exit(EXIT_FAILURE);
	}

	// Need to get everypage from AA to ZZ
	for (i = 'A'; i <= 'Z'; ++i) {
		for (j = 'A'; j <= 'Z'; ++j) {
			if ((fprintf(batch_file, "%c%c.txt ", i, j)) < 0) {
				perror("fprintf");
				exit(EXIT_FAILURE);
			}	
		}
	}	

	// Put the beginning of the grep command into the batch file
	if ((fprintf(batch_file, "> allword.txt\n")) < 0) {
		perror("fprintf");
		exit(EXIT_FAILURE);
	}

}

int main(int argc, char *argv[]) {
	FILE *batch_file;

	batch_file = fopen("word_sorter.bat", "w+");	
	if (batch_file < 0) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	save_wiki_pages(batch_file);
	extract_text(batch_file);
	find_words(batch_file);

	fclose(batch_file);

	return 1;
}