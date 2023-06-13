#include <stdio.h>
#include <conio.h>
#include <direct.h>
#include "utilFile.h"
#include "utilElevate.h"
#include "utilRegistry.h"
#include "utilStdInclude.h"
#include "utilString.h"
#include "utilSystem.h"

char **files;
void mapFunc(const char* fname, bool isHidden, bool isDir, void* userData)
{
	arrayPush(&files, strdup(fname));
}

char *fileTempName(char *buf, size_t buf_size)
{
	char temp_path[MAX_PATH] = "./tmp";
	GetTempPath(ARRAY_SIZE(temp_path), temp_path);
	char temp_file[MAX_PATH] = "";
	GetTempFileName(temp_path, "libGlov", 0, temp_file);
	strcpy_s(buf, buf_size, temp_file);
	return buf;
}

bool doRename(const char *src, const char *dest)
{
	printf("  %s -> %s\n", src, dest);
	if (!MoveFileEx(src, dest, 0)) {
		printf("Error renaming %s to %s: %d\n", src, dest, GetLastError());
		return true;
	}
	return false;
}

void pak() {
	_getch();
	while (_kbhit())
		_getch();
}

int autoRegister()
{
	char value[MAX_PATH];
	sprintf_s(value, "%s %%1", getExecutableFullPath());
	backSlashes(value);
	RegSetResult rsr = regSetString("HKEY_CLASSES_ROOT\\Directory\\shell\\Renamer\\command\\", value);
	if (rsr == RSR_FAILED && !isElevated() && canElevate()) {
		elevate("");
		return 1;
	}
	msgBox("Renamer", (rsr == RSR_SET) ? "Registered shell hooks" : (rsr == RSR_ALREADY_SET) ? "Shell hooks already registered" : "Failed to register shell hooks", MB_OK);
	return 0;
}


int main(int argc, char **argv)
{
	if (argc < 2) {
		return autoRegister();
	}
	char dir[MAX_PATH];
	strcpy_s(dir, argv[1]);
	for (int ii = 2; ii < argc; ii++)
	{
		strcat_s(dir, " ");
		strcat_s(dir, argv[ii]);
	}

	mapDirFiles(dir, mapFunc, NULL);
	arrayQSort(files, cmpStringForSort);

	if (!arraySize(&files)) {
		printf("No files found\n");
		pak();
		return 0;
	}

	_chdir(dir);

	char temp_file[MAX_PATH];
	fileTempName(temp_file, ARRAY_SIZE(temp_file));
	strcat_s(temp_file, ".renamer");

	FILE *f = NULL;
	fopen_s(&f, temp_file, "w");
	for (int ii = 0; ii < arraySize(&files); ii++)
	{
		fprintf_s(f, "%s\n", files[ii]);
	}
	fclose(f);

	char *orig_data = fload(temp_file, "r", NULL);

retry:

	char cmd1[MAX_PATH];
	sprintf_s(cmd1, "SublimeTextWait \"%s\"", temp_file);
	int ret = system(cmd1);
	if (ret != 0) {
		printf("SublimeTextWait not found, trying associated editor instead...\n");
		printf("Opening %s with associated editor...\n", temp_file);
		fileOpenWithEditor(temp_file);
	}

	char *data = fload(temp_file, "r", NULL);
	if (strcmp(data, orig_data) != 0) {
		// user made a change, must be a synchronous editor
	} else {
		printf("Press any key when ready to apply changes...\n");

		pak();

		data = fload(temp_file, "r", NULL);
	}

	char *cur = data;
	char *context = NULL;
	char *next;
	char **new_files = NULL;
	while ((next = strtok_s(cur, "\r\n", &context))) {
		cur = NULL;
		arrayPush(&new_files, next);
	}
	if (arraySize(&new_files) != arraySize(&files)) {
		printf("Mismatched number of lines, try again.\n");
		goto retry;
	}

	bool any_change = false;
	bool bad_rename = false;
	bool *use_temp = callocStructs(bool, arraySize(&new_files));
	for (int ii = 0; ii < arraySize(&files); ii++)
	{
		if (strcmp(files[ii], new_files[ii]) != 0) {
			bool need_temp = false;
			for (int jj = 0; jj < arraySize(&new_files); jj++)
			{
				if (ii != jj && stricmp(new_files[ii], new_files[jj]) == 0) {
					bad_rename = true;
					printf("Two files being renamed to %s: %s and %s\n", new_files[ii], files[ii], files[jj]);
					break;
				}
				if (ii != jj && stricmp(new_files[ii], files[jj]) == 0) {
					need_temp = true;
				}
			}
			if (need_temp) {
				use_temp[ii] = true;
				printf("  %s -> temp -> %s\n", files[ii], new_files[ii]);
			} else {
				printf("  %s -> %s\n", files[ii], new_files[ii]);
			}
			any_change = true;
		}
	}

	if (bad_rename) {
		printf("Bad rename, try again.\n");
		goto retry;
	}

	if (!any_change) {
		printf("No change detected, quitting.\n");
	} else {
		printf("Press any key to apply changes, close window to cancel...\n");
		pak();
		int temp_base = GetCurrentProcessId();
		char temp_name[MAX_PATH];
		bool abort = false;
		for (int ii = 0; !abort && ii < arraySize(&files); ii++)
		{
			if (strcmp(files[ii], new_files[ii]) != 0) {
				if (use_temp[ii]) {
					sprintf_s(temp_name, "renamer.%d.%d.tmp", temp_base, ii);
					abort |= doRename(files[ii], temp_name);
				} else {
					abort |= doRename(files[ii], new_files[ii]);
				}
			}
		}
		for (int ii = 0; !abort && ii < arraySize(&files); ii++)
		{
			if (strcmp(files[ii], new_files[ii]) != 0) {
				if (use_temp[ii]) {
					sprintf_s(temp_name, "renamer.%d.%d.tmp", temp_base, ii);
					abort |= doRename(temp_name, new_files[ii]);
				}
			}
		}
	}

	_unlink(temp_file);
	printf("Press any key to exit...\n");
	pak();
	return 0;
}
