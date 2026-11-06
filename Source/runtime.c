#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <tlhelp32.h>
#include "MinHook.h"

/* This module changes two script conditions in the verified compiler input
 * buffer. It never writes an executable, Pak, save, or script-checksum table. */
typedef struct { wchar_t *data; int32_t num; int32_t capacity; } FStringView;
typedef bool (*VerifyFn)(const FStringView *, FStringView *);
static VerifyFn original_verify;
static HMODULE own_module;
static volatile LONG initialized;
static volatile LONG character_count, pickup_count;
static wchar_t log_path[32768];
static unsigned char *absolute_block;
static const unsigned char compiler_entry[16]={0x48,0x89,0x5c,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
static const char supported_exe[]="4415933ce33476f14c837abb61a1b60af8b78cd81783896501913a7aac69e423";
static const char character_hash[]="85073798f19c717f7ee8daa82f7264842250ebe24f5c08a8466e2c0d7e0b4c3d";
static const char pickup_hash[]="8fcb49cffcce04b2c8857ad3b2745ea8643f379c216a3b0d6e5d9e0360c9015c";
static const wchar_t character_needle[]=L"if (sameTypeCount >= 3)";
static const wchar_t pickup_needle[]=L"newAccData.AccessoryType, accessorySlotIndex) >= 3)";

static void log_line(const char *message) {
 if(!log_path[0]) return;
 HANDLE file=CreateFileW(log_path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(file==INVALID_HANDLE_VALUE)return;
 SYSTEMTIME now; GetLocalTime(&now);char line[1024];
 int len=snprintf(line,sizeof(line),"%02u:%02u:%02u [PBZAccessoryTypeUnlock] %s\r\n",now.wHour,now.wMinute,now.wSecond,message);
 DWORD n;if(len>0)WriteFile(file,line,(DWORD)((len<(int)sizeof(line))?len:(int)sizeof(line)-1),&n,NULL);
 CloseHandle(file);
}
static bool hash_start(BCRYPT_ALG_HANDLE *alg,BCRYPT_HASH_HANDLE *hash,void **object) {
 DWORD size=0,got=0;
 *alg=NULL;*hash=NULL;*object=NULL;
 if(BCryptOpenAlgorithmProvider(alg,BCRYPT_SHA256_ALGORITHM,NULL,0)<0)return false;
 if(BCryptGetProperty(*alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&size,sizeof(size),&got,0)<0)return false;
 *object=HeapAlloc(GetProcessHeap(),0,size);if(!*object)return false;
 return BCryptCreateHash(*alg,hash,(PUCHAR)*object,size,NULL,0,0)>=0;
}
static void hash_close(BCRYPT_ALG_HANDLE alg,BCRYPT_HASH_HANDLE hash,void *object) {
 if(hash)BCryptDestroyHash(hash);
 if(object)HeapFree(GetProcessHeap(),0,object);
 if(alg)BCryptCloseAlgorithmProvider(alg,0);
}
static void as_hex(const unsigned char bytes[32],char text[65]) {
 static const char chars[]="0123456789abcdef";
 for(int i=0;i<32;i++){text[i*2]=chars[bytes[i]>>4];text[i*2+1]=chars[bytes[i]&15];}text[64]=0;
}
static bool hash_bytes(const unsigned char *data,DWORD length,char text[65]) {
 BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;void *object=NULL;unsigned char digest[32];bool ok=false;
 if(hash_start(&alg,&hash,&object)&&BCryptHashData(hash,(PUCHAR)data,length,0)>=0&&BCryptFinishHash(hash,digest,32,0)>=0){as_hex(digest,text);ok=true;}
 hash_close(alg,hash,object);return ok;
}
static bool hash_file(const wchar_t *path,char text[65]) {
 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
 if(f==INVALID_HANDLE_VALUE)return false;
 BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;void *object=NULL;unsigned char digest[32];bool ok=hash_start(&alg,&hash,&object);
 unsigned char *buffer=HeapAlloc(GetProcessHeap(),0,1024*1024);if(!buffer)ok=false;
 while(ok){DWORD n=0;if(!ReadFile(f,buffer,1024*1024,&n,NULL)){ok=false;break;}if(!n)break;if(BCryptHashData(hash,buffer,n,0)<0){ok=false;break;}}
 if(ok&&BCryptFinishHash(hash,digest,32,0)>=0)as_hex(digest,text);else ok=false;
 if(buffer)HeapFree(GetProcessHeap(),0,buffer);
 hash_close(alg,hash,object);CloseHandle(f);return ok;
}
static bool valid_string(const FStringView *s,int max) {
 return s&&s->data&&s->num>0&&s->num<=max&&s->capacity>=s->num&&s->data[s->num-1]==0;
}
static bool is_script(const FStringView *path,const wchar_t *suffix) {
 if(!valid_string(path,32768))return false;
 size_t len=(size_t)path->num-1,want=wcslen(suffix);if(len<want)return false;
 const wchar_t *tail=path->data+len-want;
 for(size_t i=0;i<want;i++){
  wchar_t a=tail[i],b=suffix[i];if(a==L'\\')a=L'/';if(a>=L'A'&&a<=L'Z')a+=L'a'-L'A';if(a!=b)return false;
 }
 return true;
}
static bool writable_buffer(void *start,size_t size) {
 uintptr_t current=(uintptr_t)start,end=current+size;if(!size||end<current)return false;
 while(current<end){MEMORY_BASIC_INFORMATION m;if(!VirtualQuery((void *)current,&m,sizeof(m)))return false;
  DWORD p=m.Protect&0xff;if(m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD)||!(p==PAGE_READWRITE||p==PAGE_WRITECOPY||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY))return false;
  uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=current)return false;current=next;
 }return true;
}
static bool apply_verified_source(const FStringView *path,FStringView *source) {
 const wchar_t *needle=NULL;const char *expected=NULL;volatile LONG *counter=NULL;const char *name=NULL;
 if(is_script(path,L"/ui/widget/character_v2.js")){needle=character_needle;expected=character_hash;counter=&character_count;name="Character_V2.js";}
 else if(is_script(path,L"/ui/widget/pickupweapon.js")){needle=pickup_needle;expected=pickup_hash;counter=&pickup_count;name="PickupWeapon.js";}
 else return false;
 if(!valid_string(source,500000)){log_line("source buffer layout differs; original behavior retained");return false;}
 int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,source->data,source->num-1,NULL,0,NULL,NULL);
 if(bytes<1||bytes>2000000)return false;
 unsigned char *utf8=HeapAlloc(GetProcessHeap(),0,(SIZE_T)bytes);if(!utf8)return false;
 bool ok=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,source->data,source->num-1,(char *)utf8,bytes,NULL,NULL)==bytes;
 char actual[65];ok=ok&&hash_bytes(utf8,(DWORD)bytes,actual)&&strcmp(actual,expected)==0;HeapFree(GetProcessHeap(),0,utf8);
 if(!ok){log_line("target script differs from supported original; original behavior retained");return false;}
 wchar_t *match=wcsstr(source->data,needle);if(!match||wcsstr(match+1,needle)){log_line("restriction pattern absent or ambiguous; original behavior retained");return false;}
 wchar_t *digit=match+wcslen(needle)-2;if(*digit!=L'3'||!writable_buffer(digit,sizeof(*digit)))return false;
 *digit=L'6';InterlockedIncrement(counter);
 char message[160];snprintf(message,sizeof(message),"APPLIED %s: category threshold 3 -> 6 after original source verification",name);log_line(message);
 return true;
}
static bool hooked_verify(const FStringView *path,FStringView *source) {
 bool allowed=original_verify(path,source);
 if(allowed)apply_verified_source(path,source);
 return allowed;
}
static void absolute_jump(unsigned char *out,const void *destination) {
 out[0]=0xff;out[1]=0x25;memset(out+2,0,4);memcpy(out+6,&destination,8);
}
static bool install_absolute_hook(void *target) {
 /* Fixed-build fallback: the inspected first 16 bytes contain only register
    saves/pushes, with no relative operands. It needs no near/RWX allocation. */
 if(memcmp(target,compiler_entry,16)!=0)return false;
 unsigned char *block=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!block)return false;
 memcpy(block,compiler_entry,16);
 /* R11 is volatile and unused by this inspected prologue. A register jump
    avoids misclassifying the continuation as a Windows x64 tail epilogue. */
 block[16]=0x49;block[17]=0xbb;void *continuation=(unsigned char *)target+16;
 memcpy(block+18,&continuation,8);block[26]=0x41;block[27]=0xff;block[28]=0xe3;
 /* Seven pushes leave RBX's original home slot at frame offset +80. */
 const unsigned char unwind[]={1,16,9,0,16,0xf0,14,0xe0,12,0xd0,10,0xc0,8,0x70,7,0x60,6,0x50,5,0x34,10,0,0,0};
 memcpy(block+64,unwind,sizeof(unwind));
 PRUNTIME_FUNCTION runtime=(PRUNTIME_FUNCTION)(block+128);
 runtime->BeginAddress=0;runtime->EndAddress=29;runtime->UnwindData=64;
 DWORD previous;
 if(!VirtualProtect(block,4096,PAGE_EXECUTE_READ,&previous)||!RtlAddFunctionTable(runtime,1,(DWORD64)block)){
  VirtualFree(block,0,MEM_RELEASE);return false;
 }
 HANDLE *threads=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(HANDLE)*2048);
 if(!threads){RtlDeleteFunctionTable(runtime);VirtualFree(block,0,MEM_RELEASE);return false;}
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);size_t count=0;bool ok=snapshot!=INVALID_HANDLE_VALUE;
 THREADENTRY32 thread;memset(&thread,0,sizeof(thread));thread.dwSize=sizeof(thread);
 DWORD process_id=GetCurrentProcessId(),self_id=GetCurrentThreadId();
 if(ok&&Thread32First(snapshot,&thread))do{
  if(thread.th32OwnerProcessID!=process_id||thread.th32ThreadID==self_id)continue;
  if(count==2048){ok=false;break;}
  HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,thread.th32ThreadID);
  if(!h){ok=false;break;}
  if(SuspendThread(h)==(DWORD)-1){CloseHandle(h);ok=false;break;}
  threads[count++]=h;CONTEXT context;memset(&context,0,sizeof(context));context.ContextFlags=CONTEXT_CONTROL;
  if(!GetThreadContext(h,&context)||(context.Rip>=(DWORD64)target&&context.Rip<(DWORD64)target+16)){ok=false;break;}
 }while(Thread32Next(snapshot,&thread));
 if(snapshot!=INVALID_HANDLE_VALUE)CloseHandle(snapshot);
 if(ok){
  DWORD old;unsigned char detour[16];memset(detour,0x90,sizeof(detour));absolute_jump(detour,(void *)&hooked_verify);
  if(VirtualProtect(target,16,PAGE_EXECUTE_READWRITE,&old)){
   original_verify=(VerifyFn)block;memcpy(target,detour,16);FlushInstructionCache(GetCurrentProcess(),target,16);
   DWORD unused;if(!VirtualProtect(target,16,old,&unused))log_line("WARNING: entry page protection could not be restored");
  }else ok=false;
 }
 for(size_t i=count;i>0;i--){ResumeThread(threads[i-1]);CloseHandle(threads[i-1]);}
 HeapFree(GetProcessHeap(),0,threads);
 if(!ok){RtlDeleteFunctionTable(runtime);VirtualFree(block,0,MEM_RELEASE);return false;}
 absolute_block=block;return true;
}
static void initialize(void) {
 if(InterlockedCompareExchange(&initialized,1,0)!=0)return;
 if(GetModuleFileNameW(own_module,log_path,32768)){
  wchar_t *last=wcsrchr(log_path,L'\\');if(last)*last=0;last=wcsrchr(log_path,L'\\');if(last)*last=0;
  if(wcslen(log_path)+40<32768)wcscat(log_path,L"\\PBZAccessoryTypeUnlock.log");else log_path[0]=0;
 }
 log_line("START UE4SS runtime mod 1.0.1; executable and game files will not be written");
 wchar_t exe[32768];char hash[65];
 if(!GetModuleFileNameW(NULL,exe,32768)||!hash_file(exe,hash)||strcmp(hash,supported_exe)!=0){log_line("DISABLED: unsupported or modified PBZ.exe; no hook installed");return;}
 unsigned char *base=(unsigned char *)GetModuleHandleW(NULL);void *target=base+0x062f65e0;
 unsigned char *entry=(unsigned char *)target;
 if(memcmp(entry,compiler_entry,16)!=0){
  if(entry[0]==0xe9||(entry[0]==0xff&&entry[1]==0x25))log_line("Existing runtime detour detected; preserving its behavior through hook chaining");
  else {log_line("DISABLED: compiler entry is not the inspected function");return;}
 }
 MH_STATUS status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED){log_line("DISABLED: MinHook initialization failed");return;}
 HMODULE pinned=NULL;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&hooked_verify,&pinned)){log_line("DISABLED: could not retain runtime module");return;}
 status=MH_CreateHook(target,(void *)&hooked_verify,(void **)&original_verify);
 if(status==MH_ERROR_MEMORY_ALLOC&&install_absolute_hook(target))log_line("Using verified absolute-jump runtime hook with registered unwind metadata");
 else if(status!=MH_OK){char m[100];snprintf(m,sizeof(m),"DISABLED: hook creation failed (%d)",(int)status);log_line(m);return;}
 else {
  status=MH_EnableHook(target);
  if(status!=MH_OK){MH_RemoveHook(target);log_line("DISABLED: hook activation failed");return;}
 }
 log_line("READY: waiting for the two original scripts; checksum records unchanged");
}
__declspec(dllexport) int luaopen_pbz_accessory_runtime(void *lua_state) {
 (void)lua_state;initialize();return 0;
}
#ifdef PBZ_TEST_EXPORT
__declspec(dllexport) int pbz_test_apply(const FStringView *path,FStringView *source) {
 return apply_verified_source(path,source)?1:0;
}
__declspec(dllexport) int pbz_test_absolute_install(void *target) { return install_absolute_hook(target)?1:0; }
__declspec(dllexport) int pbz_test_unwind(void) {
 if(!absolute_block)return 0;
 DWORD64 stack[24];for(int i=0;i<24;i++)stack[i]=0x1000+(DWORD64)i;
 for(int offset=16;offset<=26;offset+=10){
  CONTEXT c;memset(&c,0,sizeof(c));c.ContextFlags=CONTEXT_FULL;c.Rip=(DWORD64)absolute_block+offset;c.Rsp=(DWORD64)stack;
  PVOID data=NULL;DWORD64 frame=0;
  RtlVirtualUnwind(0,(DWORD64)absolute_block,c.Rip,(PRUNTIME_FUNCTION)(absolute_block+128),&c,&data,&frame,NULL);
  if(!(c.R15==stack[0]&&c.R14==stack[1]&&c.R13==stack[2]&&c.R12==stack[3]&&c.Rdi==stack[4]&&c.Rsi==stack[5]&&c.Rbp==stack[6]&&c.Rbx==stack[10]&&c.Rip==stack[7]&&c.Rsp==(DWORD64)(stack+8)))return 0;
 }
 return 1;
}
#endif
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
 (void)reserved;if(reason==DLL_PROCESS_ATTACH){own_module=instance;DisableThreadLibraryCalls(instance);}return TRUE;
}

