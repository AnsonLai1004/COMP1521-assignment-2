////////////////////////////////////////////////////////////////////////
// COMP1521 21T3 --- Assignment 2: `chicken', a simple file archiver
// <https://www.cse.unsw.edu.au/~cs1521/21T3/assignments/ass2/index.html>
//
// Written by YOUR-NAME-HERE (z5361987) on NOV 2021.
//
// 2021-11-05   v1.0    Team COMP1521 <cs1521 at cse.unsw.edu.au>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "chicken.h"


// ADD ANY extra #defines HERE


// ADD YOUR FUNCTION PROTOTYPES HERE
int hash_pos(int begin_pos, char *pathname);
int length_pathname(int begin_pos, char *pathname);
void print_fname(int begin_pos, int len, char *pathname);
uint64_t length_content(int begin_pos, char *pathname);
void ch_permissions(char *pathname, int a_permit_pos, char *filename);
void get_permission_string(char *p_string, int len, char *pathname);
int calculate_hash(int begin_pos, int h_pos, char *pathname);
// print the files & directories stored in egg_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes are also printed (subset 0)
//
// if long_listing is non-zero then egglet hashes should also be checked (subset 1)

void list_egg(char *egg_pathname, int long_listing) {
    FILE *f = fopen(egg_pathname, "r");
    if (f == NULL) {
        perror(egg_pathname);
        return;
    }

    while (fgetc(f) != EOF) {       
        int format = fgetc(f);
        int permissions[10];
        for (int i = 0; i < 10; i++) {
            permissions[i] = fgetc(f);
        }
        // pathname
        int lpn_pos = ftell(f); 
        int len_p = length_pathname(lpn_pos, egg_pathname);
        fseek(f, len_p + 2, SEEK_CUR);        
        
        // content length
        uint64_t len_c = length_content(ftell(f), egg_pathname);
        // content length + content + hash
        fseek(f, len_c + 7, SEEK_CUR);
       
        // long_listing
        if (long_listing) {
            for (int i = 0; i < 10; i++) {
                printf("%c", permissions[i]);
            }
            printf("  %c", format);                  
            printf("  %5lu  ", len_c);
        } 
        print_fname(lpn_pos + 2, len_p, egg_pathname);
        printf("\n");   
    }
    fclose(f);
}


// check the files & directories stored in egg_pathname (subset 1)
//
// prints the files & directories stored in egg_pathname with a message
// either, indicating the hash byte is correct, or
// indicating the hash byte is incorrect, what the incorrect value is and the correct value would be

void check_egg(char *egg_pathname) {
    FILE *f = fopen(egg_pathname, "r");
    if (f == NULL) {
        perror(egg_pathname);
        return;
    }

    int h;         
    
    int first_byte;
    while ((first_byte = fgetc(f)) != EOF) {
        // check first byte is 0x63       
        if (first_byte != 0x63) {
            fprintf(stderr, "error: incorrect first egglet byte: %#x should be 0x63\n", first_byte);
            return;
        }
        // calculate the position of pathname for later printing filenmae use
        fseek(f, 11, SEEK_CUR);
        int lpn_pos = ftell(f);
        int size = length_pathname(lpn_pos, egg_pathname);
        fseek(f, -12, SEEK_CUR);
        // calculate the hash position
        h = hash_pos(ftell(f), egg_pathname);
        // use egglet_hash function
        int c;
        uint8_t byte_value;
        uint8_t curr_hash = 0x00;
        int curr_pos = ftell(f);      
        for (int i = 0; i < (h - curr_pos); i++) {
            c = fgetc(f);
            byte_value = (uint8_t)c;
            curr_hash = egglet_hash(curr_hash, byte_value);        
        }        
        byte_value = fgetc(f);  
        // print filename
        
        print_fname(lpn_pos + 2, size, egg_pathname);
        if (byte_value == curr_hash) {
            printf(" - correct hash\n");
        } else {
            printf(" - incorrect hash %#x should be %#x\n"
            , curr_hash, byte_value);                
        }           
    }
    fclose(f);
    return;
    
}


// extract the files/directories stored in egg_pathname (subset 2 & 3)

void extract_egg(char *egg_pathname) {
    FILE *extract = fopen(egg_pathname, "r");
    if (extract == NULL) {
        perror(egg_pathname);
        return;
    }
    // create a file from the pathname in the egglet   
    while (fgetc(extract) != EOF) {
    
        fseek(extract, 1, SEEK_CUR);  
        int permissions_pos = ftell(extract);
        fseek(extract, 10, SEEK_CUR);
        int pn_len = length_pathname(ftell(extract), egg_pathname);
        char filename[pn_len + 1];
        fseek(extract, 2, SEEK_CUR);
        for (int i = 0; i < pn_len; i++) {
            filename[i] = fgetc(extract);
        }        
        filename[pn_len] = '\0';
        FILE *new = fopen(filename, "w");
        // populate the file with the content fron the egglet   
        uint64_t c_len = length_content(ftell(extract), egg_pathname);
        fseek(extract, 6, SEEK_CUR);
        for (int i = 0; i < c_len; i++) {
            int c = fgetc(extract);
            fputc(c, new);
        }
      
        // change the permissions of new
        ch_permissions(egg_pathname, permissions_pos, filename);
        
        // print out the extraction message
        printf("Extracting: %s\n", filename);
        fseek(extract, 1, SEEK_CUR);
        fclose(new);
    }
    
    
    fclose(extract);
    
    return;
}


// create egg_pathname containing the files or directories specified in pathnames (subset 3)
//
// if append is zero egg_pathname should be over-written if it exists
// if append is non-zero egglets should be instead appended to egg_pathname if it exists
//
// format specifies the egglet format to use, it must be one EGGLET_FMT_6,EGGLET_FMT_7 or EGGLET_FMT_8

void create_egg(char *egg_pathname, int append, int format,
                int n_pathnames, char *pathnames[n_pathnames]) {
    // create egg
    FILE *egg;
    if (append != 0) {
        egg = fopen(egg_pathname, "a");     
    } else {
        egg = fopen(egg_pathname, "w");
    }
    if (egg == NULL) {
        perror(egg_pathname);
        return;
    }
    
    for (int i = 0; i < n_pathnames; i++) {
        
        FILE *f = fopen(pathnames[i], "r");
        if (f == NULL) {
            perror(pathnames[i]);
            return;
        }
        fputc(0x63, egg);
        int egg_begin_pos = ftell(egg) - 1;       
        fputc(format, egg);
        // permission
        char permit[10] = {0};
        get_permission_string(permit, 10, pathnames[i]);
        for (int j = 0; j < 10; j++) {
            fputc(permit[j], egg);
        }
        // pathname len
        uint16_t pn_len = strlen(pathnames[i]);    
        char byte[2];
        for (int j = 0; j < 2; j++) {
            byte[j] = pn_len >> (j * 8);
            fputc(byte[j], egg);
        }
        // pathname
        for (int j = 0; j < pn_len; j++) {
            fputc(pathnames[i][j], egg);
        }
        // content len
        fseek(f, 0, SEEK_END);
        uint64_t c_len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char c_byte[6];
        for (int j = 0; j < 6; j++) {
            c_byte[j] = c_len >> (j * 8);
            fputc(c_byte[j], egg);
        }
        // content
        for (int j = 0; j < c_len; j++) {
            int c = fgetc(f);
            fputc(c, egg);
        }
        fflush(egg);
        // hash
        int h_pos = ftell(egg);
        int hash_value = calculate_hash(egg_begin_pos, h_pos, egg_pathname);
        fputc(hash_value, egg);
        fclose(f);
        printf("Adding: %s\n", pathnames[i]);
    }
    fclose(egg);
    return;    
}


// ADD YOUR EXTRA FUNCTIONS HERE
// give the begin position of a file, find the hash position
int hash_pos(int begin_pos, char *pathname) {
    FILE *f = fopen(pathname, "r");
    fseek(f, begin_pos, SEEK_SET);
    int hash_pos;   
    fseek(f, 12, SEEK_CUR);
    // calculate pathname_length
    int size = length_pathname(ftell(f), pathname);
    fseek(f, size + 2, SEEK_CUR);
    // calculate content_length
    uint64_t len_c = length_content(ftell(f), pathname);
    fseek(f, len_c + 6, SEEK_CUR);
    hash_pos = ftell(f);
    
    fclose(f);
    return hash_pos;
}

// give the position of the pathname length, calculate the size 
int length_pathname(int begin_pos, char *pathname) {
    FILE *f = fopen(pathname, "r");
    fseek(f, begin_pos, SEEK_SET);
    
    int f_byte = fgetc(f);        
    int s_byte = fgetc(f);    
    s_byte = s_byte << 8;   
    f_byte = f_byte | s_byte;
    fclose(f);
    return f_byte;
}

// print pathname
void print_fname(int begin_pos, int len, char *pathname) {
    FILE *f = fopen(pathname, "r");
    fseek(f, begin_pos, SEEK_SET);
    
    for (int i = 0; i < len; i++) {
        int name = fgetc(f);
        printf("%c", name);
    }    
    fclose(f);
    return;
}

// give the position of content length, calculate the size
uint64_t length_content(int begin_pos, char *pathname) {
    FILE *f = fopen(pathname, "r");
    fseek(f, begin_pos, SEEK_SET);

    uint64_t content_length = 0;
    for (int i = 0; i < 6; i++) {
        uint64_t d = fgetc(f);
        d = d << i * 8;
        content_length = content_length | d;
    } 
    fclose(f);
    return content_length;   
}

// copy permissions from file a to b 
void ch_permissions(char *pathname, int a_permit_pos, char *filename) {
    FILE *f = fopen(pathname, "r");
    fseek(f, a_permit_pos, SEEK_SET);    
    char permissions[10];
    for (int i = 0; i < 10; i++) {
        int c = fgetc(f);
        permissions[i] = (char)c;
 
    }
    mode_t mode = 0;
    if(permissions[1] == 'r') {
        mode |= S_IRUSR;
    }
    if(permissions[2] == 'w') {
        mode |= S_IWUSR;
    }
    if(permissions[3] == 'x') {
        mode |= S_IXUSR;
    }
    if(permissions[4] == 'r') {
        mode |= S_IRGRP;
    }
    if(permissions[5] == 'w') {
        mode |= S_IWGRP;
    }
    if(permissions[6] == 'x') {
        mode |= S_IXGRP;
    }
    if(permissions[7] == 'r') {
        mode |= S_IROTH;
    }
    if(permissions[8] == 'w') {
        mode |= S_IWOTH;
    }
    if(permissions[9] == 'x') {
        mode |= S_IXOTH;
    }
    if (chmod(filename, mode) != 0) {
        perror(filename);
        
        return;
    }
    return;      
}

// convert mode octal into permission string
void get_permission_string(char *p_string, int len, char *pathname) {
    for (int i = 0; i < len; i++) {
        p_string[i] = '-';
    }
    struct stat info; 
    if (stat(pathname, &info) != 0) {
        perror(pathname);
        return;
    }
    mode_t mode = info.st_mode;
    if (S_ISDIR(mode) == 1) {
        p_string[0] = 'd';
    }
    if ((mode & S_IRUSR) != 0) {
        p_string[1] = 'r';
    }
    if ((mode & S_IWUSR) != 0) {
        p_string[2] = 'w';
    }
    if ((mode & S_IXUSR) != 0) {
        p_string[3] = 'x';
    }
    if ((mode & S_IRGRP) != 0) {
        p_string[4] = 'r';
    }
    if ((mode & S_IWGRP) != 0) {
        p_string[5] = 'w';
    }
    if ((mode & S_IXGRP) != 0) {
        p_string[6] = 'x';
    }
    if ((mode & S_IROTH) != 0) {
        p_string[7] = 'r';
    }
    if ((mode & S_IWOTH) != 0) {
        p_string[8] = 'w';
    }
    if ((mode & S_IXOTH) != 0) {
        p_string[9] = 'x';
    }
    return;
}

// calculate the hash value 
int calculate_hash(int begin_pos, int h_pos, char *pathname) {
    FILE *f = fopen(pathname, "r");
    if (f == NULL) {
        perror(pathname);
        return 0;
    }
    fseek(f, begin_pos, SEEK_SET);
    int c;
    uint8_t byte_value;
    uint8_t curr_hash = 0x00;
   
    for (int i = 0; i < (h_pos - begin_pos); i++) {
        c = fgetc(f);
        byte_value = (uint8_t)c;       
        curr_hash = egglet_hash(curr_hash, byte_value);
               
    }       
    fclose(f);
    return curr_hash;
}
