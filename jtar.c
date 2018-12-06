/*
   Jacob Mendenhall
   Lab 5 -- jtar.c

   this lab is similar to the real tar.
   it takes the files and directories on 
   the command line to tar up, then prints
   everything to stdout and then on extract,
   it takes the tarFile on stdin and untars
   every file/directory back to its state before
   being tarred
*/
#include <sys/types.h>
#include <utime.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include "dllist.h"
#include "jrb.h"
#include "jval.h"
#include <string.h>

void processFiles(char *file, JRB iNode, JRB realPath);

int main(int argc, char **argv)
{
   int i, tarLength, exists;
   struct stat fileInfo;
   struct stat *infoPtr, *infoPtr2;
   JRB iNode, realPath, tmp;
   char real[PATH_MAX+1];
   char readIn[PATH_MAX+1];
   FILE *fp;
   struct utimbuf timeBuf;
   char *bytes, file[PATH_MAX+1];
   JRB directories;

   /* get c or x to tar or untar */
   tarLength = strlen(argv[1]);

   /* Error checking */
   if(argv[1][0] != 'c' && argv[1][0] != 'x')
   {
      fprintf(stderr, "Error: don't know compress or uncompress\n");
      exit(1);   
   } 
   if(tarLength == 2 && argv[1][1] != 'v')
   {
      fprintf(stderr, "Error: should only be verbose\n");
      exit(1);
   }
   if(tarLength > 2) 
   {
      fprintf(stderr, "Error: too many arguments\n");
      exit(1);   
   }

   iNode = make_jrb();
   realPath = make_jrb();
   /* compress the files together */
   if(argv[1][0] == 'c' || strcmp(argv[1], "cs") == 0)
   {
      for(i = 2; i < argc; i++)
      {
         exists = lstat(argv[i], &fileInfo);
         if(exists < 0) fprintf(stderr, "Couldn't stat\n");
         strcpy(file, argv[i]);
         /* if it is a directory, print its 
          * name and stat buff */
         if(S_ISDIR(fileInfo.st_mode))
         {
            if(realpath(file, real) == NULL)
            {
               fprintf(stderr, "Error: no path\n");
               exit(1);
            }
            if(jrb_find_str(realPath, real) == NULL)
            {
               jrb_insert_str(realPath, strdup(real), new_jval_i(0));
               fwrite(file, PATH_MAX+1, 1, stdout);
               fwrite(&fileInfo, sizeof(struct stat), 1, stdout);
            }
         }
         /* call processFile on everything */
         processFiles(file, iNode, realPath);
      }
      /* free up the strdup on key */
      jrb_traverse(tmp, realPath)
      {
         free(tmp->key.s);
      }
   }
   else if(argv[1][0] == 'x' || strcmp(argv[1], "xv") == 0)
   {
      directories = make_jrb();
      /* read in every file/directory in the tarFile */
      while(fread(readIn, PATH_MAX+1, 1, stdin) != 0)
      {
         /* get a pointer to a stat buf, to insert into directories
          * tree, so can update the chmod and time on directories */
         infoPtr = malloc(sizeof(struct stat));
         fread(infoPtr, sizeof(struct stat), 1, stdin);
         if(S_ISDIR(infoPtr->st_mode))
         {
            /* make directory and insert into tree, for 
             * after all directories and files are made
             * go back and change mods and times on directory */
            mkdir(readIn, 0777);
            jrb_insert_str(directories, strdup(readIn), new_jval_v(infoPtr));
         }
         if(S_ISREG(infoPtr->st_mode))
         {
            /* see if there is hard link, if so they'll have 
               the same iNode number in the iNode tree */
            tmp = jrb_find_int(iNode, infoPtr->st_ino);
            if(tmp == NULL)
            {
               /* insert into iNode tree, get a char* for 
                * reading the file contents, then create file and 
                * write the char* to it, close then update the mod
                * and times to back what they were when tarred */
               jrb_insert_int(iNode, infoPtr->st_ino, new_jval_s(strdup(readIn)));
               bytes = malloc(sizeof(char)*infoPtr->st_size);
               fread(bytes, infoPtr->st_size, 1, stdin);
               fp = fopen(readIn, "w");
               fwrite(bytes, infoPtr->st_size, 1, fp);
               fclose(fp);
               free(bytes);
               chmod(readIn, infoPtr->st_mode);
               timeBuf.actime = infoPtr->st_atime;
               timeBuf.modtime = infoPtr->st_mtime;
               utime(readIn, &timeBuf);
            }
            /* hard link, so link them */
            else 
            {
               link(tmp->val.s, readIn);
            }
            free(infoPtr);
         }
      }
      jrb_traverse(tmp, directories)
      {
         /* change mod and time of every directory
          * back to what they were before being tarred */
         infoPtr2 = (struct stat*) tmp->val.v;
         chmod(tmp->key.s, infoPtr2->st_mode);
         timeBuf.actime = infoPtr2->st_atime;
         timeBuf.modtime = infoPtr2->st_mtime;
         utime(tmp->key.s, &timeBuf);
         /* free memory */
         free(tmp->key.s);
         free(tmp->val.v);
      }
      /* free up the strdup on val */
      jrb_traverse(tmp, iNode)
      {
         free(tmp->val.s);
      }
      jrb_free_tree(directories);
   }
   jrb_free_tree(iNode);
   jrb_free_tree(realPath);
   return 0;
}

void processFiles(char *file, JRB iNode, JRB realPath)
{
   struct stat fileInfo;
   DIR* d;
   struct dirent* di;
   int exists;
   char path[PATH_MAX+1], real[PATH_MAX+1];
   Dllist tmp, list;
   char *bytes;
   FILE *fp;

   exists = lstat(file, &fileInfo);
   if(exists < 0) fprintf(stderr, "Couldn't stat\n");
   strcpy(path, file);
   list = new_dllist();

   /* check to see if file is a file or directory */
   if(S_ISREG(fileInfo.st_mode))
   {
      if(realpath(path, real) == NULL)
      {
         fprintf(stderr, "Error: no path\n");
         exit(1);
      }
      if(jrb_find_int(iNode, fileInfo.st_ino) == NULL) 
      {
         jrb_insert_str(realPath, strdup(real), new_jval_i(0));
         jrb_insert_int(iNode, fileInfo.st_ino, new_jval_i(0));
         /* print out the file name and its stat buf */
         fwrite(path, PATH_MAX+1, 1, stdout);
         fwrite(&fileInfo, sizeof(struct stat), 1, stdout);
         /* allocate memory for reading in from the file */
         bytes = malloc(sizeof(char)*fileInfo.st_size);
         /* open the file and read it then write it to stdout */
         fp = fopen(file, "r");
         fread(bytes, sizeof(char), fileInfo.st_size, fp);
         fwrite(bytes, fileInfo.st_size, 1, stdout);
         /* close the file, so dont have too many files open,
         * the free bytes for mem leaks */
         fclose(fp);
         free(bytes);
      }
      /* check for hard links */
      else if(jrb_find_int(iNode, fileInfo.st_ino) != NULL && 
              jrb_find_str(realPath, real) == NULL)
      {
         jrb_insert_str(realPath, strdup(real), new_jval_i(0));
         /* print out the file name and its stat buf */
         fwrite(path, PATH_MAX+1, 1, stdout);
         fwrite(&fileInfo, sizeof(struct stat), 1, stdout);
      }
   }
   else if(S_ISDIR(fileInfo.st_mode))
   {  
      d = opendir(file);
      for(di = readdir(d); di != NULL; di = readdir(d))
      {
         /* get all the files/directories from the directory on 
          * the command line, obtain the path to these and 
          * append to the list to go through and 
          * call processFiles on them */
         strcpy(path, file);
         strcat(path, "/");
         strcat(path, di->d_name);
         lstat(path, &fileInfo);
         if(strcmp(di->d_name, ".") != 0 && strcmp(di->d_name, "..") != 0)
         {
            dll_append(list, new_jval_s(strdup(path)));
         }
      }
      closedir(d);
      /* loop dlist and call process files on directories/files */
      dll_traverse(tmp, list)
      {
         exists = lstat(tmp->val.s, &fileInfo);
         if(exists < 0) fprintf(stderr, "Couldn't stat\n");
         /* if it is a directory, print its 
          * name and stat buff */
         if(S_ISDIR(fileInfo.st_mode))
         {
            if(realpath(tmp->val.s, real) == NULL)
            {
               fprintf(stderr, "Error: no path\n");
               exit(1);
            }
            if(jrb_find_str(realPath, real) == NULL)
            {
               jrb_insert_str(realPath, strdup(real), new_jval_i(0));
               fwrite(tmp->val.s, PATH_MAX+1, 1, stdout);
               fwrite(&fileInfo, sizeof(struct stat), 1, stdout);
            }
         }
         /* call processFile on everything */
         processFiles(tmp->val.s, iNode, realPath);
         free(tmp->val.s);
      }
   }
   free_dllist(list);
}
