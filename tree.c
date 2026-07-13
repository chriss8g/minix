#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h> // para lstat  

void print_tree(const char *path, int depth)
{
	struct dirent *entry;    //item  
	struct stat st;     	 //file or folder
	DIR *dir;	    	 //directory tool 
	char full_path[1024];    //for string

    //Open current path
        dir = opendir(path);
	if (!dir) {
		return;
	}
    //Loop
	while ((entry = readdir(dir)) != NULL)
        {

		//skip "." and ".."
           if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
	   {
		continue;
	   }
	       
		//spaces for depth level
	   for (int i = 0; i < depth; i++)	
	   {
		printf("    ");			 // 4 spaces per level 
	   }	

		//show name of the folder or file
	   printf("|-- %s\n", entry->d_name);
		//building path
           snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

		//is it a folder?
	   if (lstat(full_path, &st) == 0)
 	   {
		if (S_ISDIR(st.st_mode)) 
		{
		print_tree(full_path, depth + 1);
		}
<<<<<<< HEAD
	    }
         }
 	 closedir(dir);
}    
       int main(int argc, char *argv[]) 
       { 
=======
	   }
       }
 	closedir(dir);
   }    
   int main(int argc, char *argv[]) 
   { 
>>>>>>> 1242f799adc1ab073ea050785f8ad0e25605551b
	const char *start_path = (argc > 1) ? argv[1] : ".";
	
	printf("%s\n", start_path);
	
	print_tree(start_path, 0);
	
	return 0;  
<<<<<<< HEAD
       }
=======
}
>>>>>>> 1242f799adc1ab073ea050785f8ad0e25605551b
