#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>

bool is_directory(const char* path)
{
    size_t len = strlen(path);
    
    for (size_t i = 0; i < len; i++)
    {
        char c = path[i];

        if (c == '.')
        {
            if (i + 1 < len)
            {
                if (path[i + 1] != '/')
                {
                    return false;
                }
            }
        }
    }

    return true;
}

typedef struct query_entry
{
	char lexeme[1024];

	char path[1024];
	size_t line;
	size_t column;
} query_entry;

typedef struct query_result
{
	query_entry* entries;
	size_t count;
	size_t capacity;
} query_result;

#define DEFAULT_QUERY_RES_SIZE 10

void query_result_init(query_result* res)
{
	if (res == NULL)
		return;
	
	res->entries = (query_entry*)malloc(sizeof(query_entry) * DEFAULT_QUERY_RES_SIZE);
	res->capacity = DEFAULT_QUERY_RES_SIZE;
	res->count = 0;
}

void query_result_add_entry(query_result* res, const query_entry* entry)
{
	if (res == NULL)
		return;

	if (res->count + 1 >= res->capacity)
	{
		size_t new_cap = res->capacity * 2;
		res->entries = realloc(res->entries, sizeof(query_entry) * new_cap);
		res->capacity = new_cap;
	}

	res->entries[res->count++] = *entry;
}

void query_result_destroy(query_result* res)
{
	if (res == NULL)
		return;
	
	free(res->entries);
}

void search(const char* path, const char* query, query_result* res)
{
	FILE* fd = fopen(path, "r");
	if (!fd)
		return;

	fseek(fd, 0, SEEK_END);
	size_t file_size = ftell(fd);
	rewind(fd);

	char* contents = (char*)malloc((sizeof(char) * file_size) + 1);
	contents[file_size] = '\0';

	fread(contents, sizeof(char), file_size, fd);

	size_t cursor = 0;
	size_t start = 0;
	size_t line = 1;

	while (cursor < file_size)
	{
		start = cursor;
		char c = contents[cursor];

		while (isspace(c))
		{
			if (c == '\n')
				line++;
			
			cursor++;
			c = contents[cursor];
		}

		while (isalnum(c) || ispunct(c))
		{
			cursor++;
			c = contents[cursor];
		}

		if (isspace(c))
		{
			const char* lexeme = &contents[start];
			size_t length = cursor - start;

			//printf("lexeme: %.*s length: %zu\n", (int)length, lexeme, length);

			size_t query_length = strlen(query);

			if (length != query_length)
				continue;

			if (memcmp(lexeme, query, query_length) != 0)
				continue;
			
			query_entry entry;
			memcpy(entry.lexeme, lexeme, length);
			entry.lexeme[length] = '\0';
			memcpy(entry.path, path, strlen(path));
			entry.path[strlen(path)] = '\0';
			entry.line = line;
			entry.column = start + 1;
			query_result_add_entry(res, &entry);
		}
	}

	free(contents);

	fclose(fd);
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <path> <query>\n", argv[0]);
        return -1;
    }

	const char* path = argv[1];
	const char* query = argv[2];

	query_result res;

	if (is_directory(path))
	{
		DIR* dir;
		struct dirent* entry;
		dir = opendir(path);

		if (dir)
		{
			while ((entry = readdir(dir)) != NULL)
			{
				bool current_dir = strcmp(entry->d_name, ".") == 0;
				bool previous_dir = strcmp(entry->d_name, "..") == 0;
				
				if (current_dir || previous_dir)
					continue;
				
				size_t len = sizeof(char) * (strlen(path) + 1 + strlen(entry->d_name) + 1);
				
				char* fullpath = (char*)malloc(len);
				fullpath[len - 1] = '\0';

				size_t offset = 0;
				
				offset += snprintf(fullpath + offset, len - offset, "%s/", path);
				offset += snprintf(fullpath + offset, len - offset, "%s", entry->d_name);

				//printf("searching: %s in %s ...\n", query, fullpath);

				search(fullpath, query, &res);

				free(fullpath);
			}

			closedir(dir);
		}
	}
	else
	{
		search(path, query, &res);
	}

	for (size_t i = 0; i < res.count; i++)
	{
		query_entry* entry = &res.entries[i];
		printf("found query '%s' @ <%s:%zu:%zu>\n", entry->lexeme, entry->path, entry->line, entry->column);
	}
}