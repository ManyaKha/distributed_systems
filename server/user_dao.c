#include "user_dao.h"
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h> 



///////////////////////////////////////////////////////////////////////////////////////////////////
// constants
///////////////////////////////////////////////////////////////////////////////////////////////////
// paths
#define STORAGE_DIR_PATH "storage/"
// names lengths
#define MAX_FILENAME_LEN 256
// delete_all_user_files
#define DELETE_ALL_USER_FILES_SUCCESS 0
#define DELETE_ALL_USER_FILES_ERR_NO_SUCH_USER 1
#define DELETE_ALL_USER_FILES_ERR_REMOVE 2
#define DELETE_ALL_USER_FILES_ERR_CLOSE_DIR 3



///////////////////////////////////////////////////////////////////////////////////////////////////
// global variables
///////////////////////////////////////////////////////////////////////////////////////////////////
pthread_mutex_t mutex_storage;



///////////////////////////////////////////////////////////////////////////////////////////////////
// init
///////////////////////////////////////////////////////////////////////////////////////////////////

int init_user_dao()
{
    // create the storage directory if it doesn't exist
    if (mkdir(STORAGE_DIR_PATH, S_IRWXU) != 0 && errno != EEXIST)   // error occured, but not 
    {                                                               // because the the folder 
        return INIT_USER_DAO_ERR_FOLDER_CREATION;                   // already existed
    }

    // initialize mutex
    if (pthread_mutex_init(&mutex_storage, NULL) != 0)
    {
        return INIT_USER_DAO_ERR_MUTEX_INIT;
    }

    return INIT_USER_DAO_SUCCESS;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// destroy
///////////////////////////////////////////////////////////////////////////////////////////////////

int destroy_user_dao()
{
    if (pthread_mutex_destroy(&mutex_storage) != 0)
    {
        return DESTROY_USER_DAO_ERR_MUTEX;
    }

    return DESTROY_USER_DAO_SUCCESS;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// create_user
///////////////////////////////////////////////////////////////////////////////////////////////////

int create_user(char* name)
{
    // create user directory path
    char dir_path[strlen(STORAGE_DIR_PATH) + strlen(name) + 1];
    strcpy(dir_path, STORAGE_DIR_PATH);
    strcat(dir_path, name);

    // create user directory
    if (mkdir(dir_path, S_IRWXU) != 0)
    {          
        if (errno == EEXIST)     
            return CREATE_USER_ERR_EXISTS;    
        else
        {
            perror("ERROR create_user - could not create directory");
            return CREATE_USER_ERR_DIRECTORY;   
        }          
    }

    return CREATE_USER_SUCCESS;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// delete_user
///////////////////////////////////////////////////////////////////////////////////////////////////

int delete_all_user_files(char* user_dir_path, int file_path_len)
{
    int res = DELETE_ALL_USER_FILES_SUCCESS;

    // delete user files
    DIR* p_user_dir = opendir(user_dir_path);
    if (p_user_dir != NULL)
    {
        struct dirent* p_next_file;
        char filepath[file_path_len + 1];

        while ((p_next_file = readdir(p_user_dir)) != NULL )
        {
            if (p_next_file->d_name[0] != '.') // ignore 'non files'
            {
                // build the path for each file in the folder
                sprintf(filepath, "%s%s", user_dir_path, p_next_file->d_name);
                if (remove(filepath) != 0)
                {
                    perror("ERROR delete_all_user_files - could not remove file");
                    return DELETE_ALL_USER_FILES_ERR_REMOVE;
                }
            }
        }
        
        if (closedir(p_user_dir) != 0)
        {
            perror("ERROR delete_all_user_files - could not close dir");
            return DELETE_ALL_USER_FILES_ERR_CLOSE_DIR;
        }
    }
    else // no such user
        res = DELETE_ALL_USER_FILES_ERR_NO_SUCH_USER;

    return res;
}



int delete_user(char* name)
{
    int res = DELETE_USER_SUCCESS;

    // acquire the storage mutex
    if (pthread_mutex_lock(&mutex_storage) == 0)
    {
        // create user directory path. + 1 because additional / to separate dir from file
        int user_folder_path_len = strlen(STORAGE_DIR_PATH) + strlen(name) + 1;
        int file_path_len = user_folder_path_len + MAX_FILENAME_LEN; 
        char dir_path[user_folder_path_len + 1]; 
        strcpy(dir_path, STORAGE_DIR_PATH);
        strcat(dir_path, name);
        strcat(dir_path, "/");

        // first delete all user's files
        int del_files_res = delete_all_user_files(dir_path, file_path_len);
        if (del_files_res == DELETE_ALL_USER_FILES_SUCCESS)
        {
            // remove user directory
            if (remove(dir_path) != 0)
                res = DELETE_USER_ERR_REMOVE_FOLDER;
        }
        else if (del_files_res == DELETE_ALL_USER_FILES_ERR_NO_SUCH_USER)
            res = DELETE_USER_ERR_NOT_EXISTS;
        else
            res = DELETE_USER_ERR_REMOVE_FILE;

        // unlock the storage mutex
        if (pthread_mutex_unlock(&mutex_storage) != 0)
        {
            res = DELETE_USER_ERR_MUTEX_UNLOCK;
            printf("ERROR delete_user - could not unlock mutex\n");
        }
    }
    else // couldn't acquire the storage mutex
    {
        res = DELETE_USER_ERR_MUTEX_LOCK;
        printf("ERROR delete_user - could not lock mutex\n");
    }

    return res;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// get_user_files_list
///////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t count_user_files(DIR* p_directory)
{
    uint32_t quantity = 0;
    struct dirent* p_next_file;

    while ((p_next_file = readdir(p_directory)) != NULL)
    {
        if (p_next_file->d_name[0] != '.') // skip files which are not part of the storage
            ++quantity;
    }

    rewinddir(p_directory);

    return quantity;
}



int get_user_files_list(char* username, char*** p_user_files, uint32_t* p_quantity)
{
    int res = GET_USER_FILES_LIST_SUCCESS;

    // create user directory path
    int user_folder_path_len = strlen(STORAGE_DIR_PATH) + strlen(username) + 1; // + 1 --> /
    char user_dir_path[user_folder_path_len + 1]; 
    strcpy(user_dir_path, STORAGE_DIR_PATH);
    strcat(user_dir_path, username);
    strcat(user_dir_path, "/");

	// open user directory
    if (pthread_mutex_lock(&mutex_storage) == 0)
    {
        DIR* p_user_dir = opendir(user_dir_path);
        if (p_user_dir != NULL)
        {
            struct dirent* p_next_file;
            char* file_name;
            *p_quantity = count_user_files(p_user_dir);
            char** user_files = malloc(*p_quantity * sizeof(char*));
            int file_idx = 0;

            while ((p_next_file = readdir(p_user_dir)) != NULL )
            {
                file_name = p_next_file->d_name;
                if (file_name[0] != '.') // ignore 'non files'
                {
                    user_files[file_idx] = malloc((strlen(file_name) + 1) * sizeof(char));
                    strcpy(user_files[file_idx], file_name);
                    ++file_idx;
                }
            }

            *p_user_files = user_files;
            
            if (closedir(p_user_dir) != 0)
            {
                perror("ERROR get_user_files_list - could not close dir");
                res = GET_USER_FILES_LIST_ERR_CLOSE_DIR;
            }
        }
        else // no such user
            res = GET_USER_FILES_LIST_ERR_NO_SUCH_USER;

        if (pthread_mutex_unlock(&mutex_storage) != 0)
        {
            res = GET_USER_FILES_LIST_ERR_MUTEX_UNLOCK;
            printf("ERROR get_user_files_list - could not unlock mutex\n");
        }
    }
    else // couldn't lock mutex
    {
        res = GET_USER_FILES_LIST_ERR_MUTEX_LOCK;
        printf("ERROR get_user_files_list - could not lock mutex\n");
    }

	return res;
}