#include <stdint.h>
/*
    encapsulates functions dealing with physicall storage.
    IMPORTANT before any operation will be performed it is required to call the init() function and
    if the file won't be used anymore then the destroy() function must be called.
*/
///////////////////////////////////////////////////////////////////////////////////////////////////
// constants
///////////////////////////////////////////////////////////////////////////////////////////////////
// init
#define INIT_USER_DAO_SUCCESS 0
#define INIT_USER_DAO_ERR_FOLDER_CREATION 1
#define INIT_USER_DAO_ERR_MUTEX_INIT 2
// destroy
#define DESTROY_USER_DAO_SUCCESS 0
#define DESTROY_USER_DAO_ERR_MUTEX 1
// create user
#define CREATE_USER_SUCCESS 0
#define CREATE_USER_ERR_EXISTS 1
#define CREATE_USER_ERR_DIRECTORY 2
// delete user
#define DELETE_USER_SUCCESS 0
#define DELETE_USER_ERR_MUTEX_LOCK 1
#define DELETE_USER_ERR_MUTEX_UNLOCK 2
#define DELETE_USER_ERR_NOT_EXISTS 3
#define DELETE_USER_ERR_REMOVE_FOLDER 4
#define DELETE_USER_ERR_REMOVE_FILE 5
// get user files list
#define GET_USER_FILES_LIST_SUCCESS 0
#define GET_USER_FILES_LIST_ERR_NO_SUCH_USER 1
#define GET_USER_FILES_LIST_ERR_MUTEX_LOCK 2
#define GET_USER_FILES_LIST_ERR_MUTEX_UNLOCK 3
#define GET_USER_FILES_LIST_ERR_CLOSE_DIR 4



int init_user_dao();
int destroy_user_dao();
/*
    creates a new user.
    Returns:
        CREATE_USER_SUCCESS - success
*/
int create_user(char* name);
int delete_user(char* name);
int get_user_files_list(char* username, char*** p_user_files, uint32_t* p_quantity);
/*
	checks if the user with the username is registered.
	Returns 1 if the user is registered and 0 if no
*/
int is_registered(char* username);