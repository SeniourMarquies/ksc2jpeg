// Project: KSC to JPEG Converter
// Author: Seniour marquies
// Date: 24-07-2025
// Description: This program converts KSC files to JPEG format by decrypting the data.
// Source : https://github.com/ko4life-net/ko/tree/master/src/tool/KscViewer
// License: GNU General Public License v3.0

/*
Features to implement
1 - Recursively search for .ksc files in subdirectories.
2 - jump if the file is already converted (check for .jpeg file with same name).
3 - Logging system to keep track of conversions.
4 - Measure-time for conversion and display it.
5 - Open file automatically after conversion.
6 - Header check to ensure the file is a valid KSC file.
7 - output file can be specified with a different name or path.
*/

#define _CRT_SECURE_NO_WARNINGS


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <stdint.h>
static uint16_t m_r = 1124;
static const uint16_t m_c1 = 52845;
static const uint16_t m_c2 = 22719;

uint8_t Decrypt(uint8_t cipher) {
	uint8_t plain = cipher ^ (m_r >> 8);
	m_r = (cipher + m_r) * m_c1 + m_c2;
}

int convert_ksc_to_jpeg(const char* ksc_path, const char* jpeg_path) {

	FILE* f_in = fopen(ksc_path, "rb");
	if (!f_in) {
		fprintf(stderr, "Failed to open input file: %s\n", ksc_path);
		return 1;
	}

	fseek(f_in, 0, SEEK_END);
	long fsize = ftell(f_in);
	rewind(f_in);
	if (fsize <= 8) {
		fprintf(stderr, "File too small: %s\n", ksc_path);
		fclose(f_in);
		return 1;
	}
	uint8_t* buffer = malloc(fsize);
	if (!buffer) {
	fprintf(stderr, "Memory allocation failed\n");
		fclose(f_in);
		return 1;
	}
	fread(buffer, 1, fsize, f_in);
	fclose(f_in);
	m_r = 1124; // Reset the random number generator
	for (int i = 0; i < 8; ++i) Decrypt(buffer[i]); // header skip

	FILE* f_out = fopen(jpeg_path, "wb");
	if(!f_out){
		fprintf(stderr, "Failed to open output file: %s\n", jpeg_path);
		free(buffer);
		return 1;
	}
	for (long i = 8; i < fsize; ++i) {
		uint8_t plain = Decrypt(buffer[i]);
		fwrite(&plain, 1, 1, f_out);
	}
	fclose(f_out);
	free(buffer);
	return 0;

}

void convert_all_ksc_in_folder() {
	// https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-win32_find_dataa
	WIN32_FIND_DATAA find_data;
	HANDLE hFind = FindFirstFileA("*.ksc", &find_data);

	if(hFind== INVALID_HANDLE_VALUE) {
		fprintf(stderr, "No .ksc files found in the current directory.\n");
		return;
	}
	do {
		const char* ksc_file = find_data.cFileName;
		char jpeg_file[MAX_PATH];
		strcpy(jpeg_file, ksc_file);
		char* dot = strrchr(jpeg_file, '.'); // find last dot in  jpeg_file
		if (dot) strcpy(dot, ".jpeg"); // replace extension with .jpeg
		else strcat(jpeg_file, ".jpeg"); // append .jpeg if no dot found

		printf("Converting %s to %s...\n", ksc_file, jpeg_file);
		if (convert_ksc_to_jpeg(ksc_file, jpeg_file) == 0) {
			printf("Successfully converted %s to %s\n", ksc_file, jpeg_file);
		} else {
			fprintf(stderr, "Failed to convert %s\n", ksc_file);
		}
	} while (FindNextFileA(hFind, &find_data) != 0);
	FindClose(hFind);


}

int main(int argc, char* argv[]) {
	if (argc == 3) {
		// Convert a single file
		return convert_ksc_to_jpeg(argv[1], argv[2]);
	} else if (argc == 1) {
		// Convert all .ksc files in the current directory
		convert_all_ksc_in_folder();
		return 0;
	} else {
		fprintf(stderr, "Usage: %s [input.ksc] [output.jpeg]\n", argv[0]);
		return 1;
	}
}