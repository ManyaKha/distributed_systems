/*
    encapsulates functions dealing with physicall storage.
    IMPORTANT before any operation will be performed it is required to call the init() function and
    if the file won't be used anymore then the destroy() function must be called.
*/
///////////////////////////////////////////////////////////////////////////////////////////////////
// constants
///////////////////////////////////////////////////////////////////////////////////////////////////
// create user
#define CREATE_USER_SUCCESS 0
// init
#define INIT_USER_DAO_SUCCESS 0
#define INIT_USER_DAO_ERR_FOLDER_CREATION 1
#define INIT_USER_DAO_ERR_MUTEX_INIT 2
// destroy
#define DESTROY_USER_DAO_SUCCESS 0
#define DESTROY_USER_DAO_ERR_MUTEX 1



int init_user_dao();
int destroy_user_dao();
/*
    creates a new user.
    Returns:
        CREATE_USER_SUCCESS - success
*/
int create_user(char* name);