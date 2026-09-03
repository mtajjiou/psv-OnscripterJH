/* -*- C++ -*-
 * 
 *  onscripter_main.cpp -- main function of ONScripter
 *
 *  Copyright (c) 2001-2018 Ogapee. All rights reserved.
 *            (C) 2014-2019 jh10001 <jh10001@live.cn>
 *            (C) 2019-2019 wetor(����W���) <maho.wang>
 *            (c) 2022-2022 Yurisizuku 
 *  ogapee@aqua.dti2.ne.jp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ONScripter.h"
#include "Utils.h"
#include "gbk2utf16.h"
#include "sjis2utf16.h"
#include "version.h"
#include "encoding_detect.h"
#include "build_version.h"

ONScripter ons;
Coding2UTF16 *coding2utf16 = NULL;

/* Set when --enc:sjis or --enc:gbk was given.  Without one, the script's
 * encoding is detected from the script itself, which is what a player who
 * does not know their game's code page needs. */
static bool coding_chosen_explicitly = false;

#if defined(IOS)
#import <Foundation/NSArray.h>
#import <UIKit/UIKit.h>
#import "DataCopier.h"
#import "DataDownloader.h"
#import "ScriptSelector.h"
#import "MoviePlayer.h"
#endif

#ifdef ANDROID
#include <unistd.h>
#endif

#ifdef WINRT
#include "ScriptSelector.h"
#endif

void optionHelp()
{
    printf( "Usage: onscripter [option ...]\n" );
    printf( "      --cdaudio\t\tuse CD audio if available\n");
    printf( "      --cdnumber no\tchoose the CD-ROM drive number\n");
    printf( "  -f, --font file\tset a TTF font file\n");
    printf( "      --registry file\tset a registry file\n");
    printf( "      --dll file\tset a dll file\n");
    printf( "  -r, --root path\tset the root path to the archives\n");
    printf( "      --fullscreen\tstart in fullscreen mode\n");
    printf( "      --window\t\tstart in windowed mode\n");
    printf( "      --force-button-shortcut\tignore useescspc and getenter command\n");
    printf( "      --enable-wheeldown-advance\tadvance the text on mouse wheel down\n");
    printf( "      --disable-rescale\tdo not rescale the images in the archives\n");
    printf( "      --render-font-outline\trender the outline of a text instead of casting a shadow\n");
    printf( "      --edit\t\tenable online modification of the volume and variables when 'z' is pressed\n");
    printf( "      --key-exe file\tset a file (*.EXE) that includes a key table\n");
    printf( "      --enc:sjis\tread the script as shift-jis (japanese)\n");
    printf( "      --enc:gbk\tread the script as gbk (chinese)\n");
    printf( "      --enc:auto\tdetect the script encoding; the default\n");
    printf( "      --debug:1\t\tprint debug info\n");
    printf( "      --fontcache\tcache default font\n");
    printf( "  -h, --help\t\tshow this help and exit\n");
    printf( "  -v, --version\t\tshow the version information and exit\n");
    exit(0);
}

void optionVersion()
{
    printf("Written by Ogapee <ogapee@aqua.dti2.ne.jp>\n\n");
    printf("Copyright (c) 2001-2018 Ogapee.\n\
                              (C) 2014-2018 jh10001<jh10001@live.cn>\n");
    printf("This is free software; see the source for copying conditions.\n");
    exit(0);
}

#if defined(PSV)
int VITA_SuspendCallback(int notifyId, int notifyCount, int powerInfo, void *common)
{
	// The resume check that used to live here tested SCE_POWER_CB_RESUMING,
	// which current vitasdk headers no longer declare (it is now
	// SCE_POWER_CB_APP_RESUMING). Its body was empty, so the test had no
	// effect and is dropped rather than rewritten against a constant whose
	// value may not match. Registering the callback is what matters; if a
	// resume handler is ever implemented, test SCE_POWER_CB_APP_RESUMING.
	(void)powerInfo;
	return 0;
}

int VITA_RegisterCallbackThread(SceSize args, void *argp)
{
	SceUID cbid;
	cbid = sceKernelCreateCallback("Suspend Callback", 0, VITA_SuspendCallback, NULL);
	scePowerRegisterCallback(cbid);
	while (1) // FIXME: to recode
	{
		sceKernelDelayThreadCB(0);
	}
	return 0;
}

int VITA_SetupCallbacks(void)
{
	SceUID thid;
	thid = sceKernelCreateThread("update_thread", VITA_RegisterCallbackThread, 0x10000100, 0x10000, 0, 0, NULL);
	if (thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}
	return thid;
}

#endif

#if defined(IOS)
extern "C" void playVideoIOS(const char *filename, bool click_flag, bool loop_flag)
{
    NSString *str = [[NSString alloc] initWithUTF8String:filename];
    id obj = [MoviePlayer alloc];
    [[obj init] play:str click : click_flag loop : loop_flag];
    [obj release];
}
#endif

void parseOption(int argc, char *argv[]) {
    while (argc > 0) 
    {
        if ( argv[0][0] == '-' )
        {
            if ( !strcmp( argv[0]+1, "h" ) || !strcmp( argv[0]+1, "-help" ) ){
                optionHelp();
            }
            else if ( !strcmp( argv[0]+1, "v" ) || !strcmp( argv[0]+1, "-version" ) ){
                optionVersion();
            }
            else if ( !strcmp( argv[0]+1, "-cdaudio" ) ){
                ons.enableCDAudio();
            }
            else if ( !strcmp( argv[0]+1, "-cdnumber" ) ){
                argc--;
                argv++;
                ons.setCDNumber(atoi(argv[0]));
            }
            else if ( !strcmp( argv[0]+1, "f" ) || !strcmp( argv[0]+1, "-font" ) ){
                argc--;
                argv++;
                ons.setFontFile(argv[0]);
            }
            else if ( !strcmp( argv[0]+1, "-registry" ) ){
                argc--;
                argv++;
                ons.setRegistryFile(argv[0]);
            }
            else if ( !strcmp( argv[0]+1, "-dll" ) ){
                argc--;
                argv++;
                ons.setDLLFile(argv[0]);
            }
            else if ( !strcmp( argv[0]+1, "r" ) || !strcmp( argv[0]+1, "-root" ) ){
                argc--;
                argv++;
                ons.setArchivePath(argv[0]);
            }
            else if ( !strcmp( argv[0]+1, "-fullscreen" ) ){
                ons.setFullscreenMode();
            }
            else if ( !strcmp( argv[0]+1, "-window" ) ){
                ons.setWindowMode();
            }
            else if ( !strcmp( argv[0]+1, "-force-button-shortcut" ) ){
                ons.enableButtonShortCut();
            }
            else if ( !strcmp( argv[0]+1, "-enable-wheeldown-advance" ) ){
                ons.enableWheelDownAdvance();
            }
            else if ( !strcmp( argv[0]+1, "-disable-rescale" ) ){
                ons.disableRescale();
            }
            else if ( !strcmp( argv[0]+1, "-render-font-outline" ) ){
                ons.renderFontOutline();
            }
            else if ( !strcmp( argv[0]+1, "-edit" ) ){
                ons.enableEdit();
            }
            else if ( !strcmp( argv[0]+1, "-key-exe" ) ){
                argc--;
                argv++;
                ons.setKeyEXE(argv[0]);
            }
            else if (!strcmp(argv[0]+1, "-enc:sjis")){
                if (coding2utf16 == NULL) coding2utf16 = new SJIS2UTF16();
                coding_chosen_explicitly = true;
            }
            else if (!strcmp(argv[0]+1, "-enc:gbk")){
                if (coding2utf16 == NULL) coding2utf16 = new GBK2UTF16();
                coding_chosen_explicitly = true;
            }
            else if (!strcmp(argv[0]+1, "-enc:auto")){
                /* The default; accepted so a front end can be explicit
                 * about wanting detection. */
            }
            else if (!strcmp(argv[0]+1, "-debug:1")){
                ons.setDebugLevel(1);
            }
            else if (!strcmp(argv[0]+1, "-fontcache")){
                ons.setFontCache();
            }
			else if (!strcmp(argv[0]+1, "-no-vsync")){
			    ons.setVsyncOff();
			}
#if defined(ANDROID) || defined(PSV)
            else if ( !strcmp(argv[0]+1, "-compatible") ){
                ons.setCompatibilityMode();
            }
            else if ( !strcmp(argv[0] + 1, "-save-dir") ){
                argc--;
                argv++;
                ons.setSaveDir(argv[0]);
            }
#if defined(PSV)
			else if (!strcmp(argv[0] + 1, "-textbox")) {
				ons.enableTextBox();
			}
			else if (!strcmp(argv[0] + 1, "-touch-mode")) {
				argc--;
				argv++;
				ons.setTouchMode(argv[0]);
			}
#endif
#endif
            else{
                utils::printInfo(" unknown option %s\n", argv[0]);
            }
        }
        else{
            optionHelp();
        }
        argc--;
        argv++;
    }
}

extern "C"
{
    int ons_main(int argc, char *argv[])
    {
        utils::printInfo("ONScripter-Jh version %s (%s, %d.%02d)\n",
            ONS_JH_VERSION, ONS_VERSION, NSC_VERSION / 100, NSC_VERSION % 100);
        /* Which build this actually is.  Without it a log cannot be told
         * apart from one produced by a vpk installed days earlier. */
        utils::printInfo("%s\n", ONS_BUILD_STRING);
        for(int i=0; i<argc; i++)
		{
			SDL_Log("## ons_main argv[%d] %s\n", i, argv[i]);
		}
    #if defined(PSV)
        ons.setCompatibilityMode();
        ons.disableRescale();
        ons.enableButtonShortCut();
        VITA_SetupCallbacks();
    #elif defined(WINRT)
        {
            ScriptSelector ss;
            ons.setArchivePath(ss.selectedPath.c_str());
        }
        ons.disableRescale();
    #elif defined(ANDROID)
        ons.enableButtonShortCut();
    #endif

    #if defined(IOS)
    #if defined(HAVE_CONTENTS)
        if ([[[DataCopier alloc] init] copy]) exit(-1);
    #endif

        // scripts and archives are stored under /Library/Caches
        NSArray* cpaths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
        NSString* cpath = [[cpaths objectAtIndex : 0] stringByAppendingPathComponent:@"ONS"];
        char filename[256];
        strcpy(filename, [cpath UTF8String]);
        ons.setArchivePath(filename);

        // output files are stored under /Documents
        NSArray* dpaths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString* dpath = [[dpaths objectAtIndex : 0] stringByAppendingPathComponent:@"ONS"];
        strcpy(filename, [dpath UTF8String]);
        ons.setSaveDir(filename);

    #if defined(ZIP_URL)
        if ([[[DataDownloader alloc] init] download]) exit(-1);
    #endif

    #if defined(USE_SELECTOR)
        // scripts and archives are stored under /Library/Caches
        cpath = [[[ScriptSelector alloc] initWithStyle:UITableViewStylePlain] select];
        strcpy(filename, [cpath UTF8String]);
        ons.setArchivePath(filename);

        // output files are stored under /Documents
        dpath = [[dpaths objectAtIndex : 0] stringByAppendingPathComponent:[cpath lastPathComponent]];
        NSFileManager *fm = [NSFileManager defaultManager];
        [fm createDirectoryAtPath : dpath withIntermediateDirectories : YES attributes : nil error : nil];
        strcpy(filename, [dpath UTF8String]);
        ons.setSaveDir(filename);
    #endif

    #if defined(RENDER_FONT_OUTLINE)
        ons.renderFontOutline();
    #endif
    #endif

        // ----------------------------------------
        // Parse options
        argv++;
        parseOption(argc - 1, argv);
        const char *argfilename = "ons_args";
        FILE *fp = NULL;
        if (ons.getArchivePath()) {
            size_t len = strlen(ons.getArchivePath()) + strlen(argfilename) + 1;
            char *full_path = new char[len];
            sprintf(full_path, "%s%s", ons.getArchivePath(), argfilename);
            fp = fopen(full_path, "r");
            delete[] full_path;
        }
        else fp = fopen(argfilename, "r");
        if (fp) {
            char **args = new char*[16];
            int argn = 0;
            args[argn] = new char[64];
            while (argn < 16 && (fscanf(fp, "%s", args[argn]) > 0)) {
                ++argn;
                if (argn < 16) args[argn] = new char[64];
            }
            parseOption(argn, args);
            for (int i = 0; i < argn; ++i) delete[] args[i];
            delete[] args;
        }

        /* Something must be in place before the script is read, so start
         * from GBK as this fork always has.  Detection below replaces it if
         * the script turns out to disagree. */
        if (coding2utf16 == NULL) coding2utf16 = new GBK2UTF16();

        // ----------------------------------------
        // Run ONScripter
        if (ons.openScript()) exit(-1);

        /* The script is decrypted but not yet parsed, so this is the one
         * moment where the raw bytes are available and swapping the codec
         * still costs nothing.  Reading a japanese game as GBK (or the
         * reverse) garbles every line and usually dies at the first piece of
         * dialogue with "text cannot be displayed in define section". */
        if (!coding_chosen_explicitly) {
            int guess = ons.guessScriptEncoding();
            if (guess == SCRIPT_ENCODING_SJIS) {
                utils::printInfo("script looks like shift-jis; reading it as japanese "
                                 "(override with --enc:gbk)\n");
                delete coding2utf16;
                coding2utf16 = new SJIS2UTF16();
            }
            else if (guess == SCRIPT_ENCODING_GBK) {
                utils::printInfo("script looks like gbk; reading it as chinese "
                                 "(override with --enc:sjis)\n");
            }
            else {
                utils::printInfo("script encoding is not clear from its bytes; "
                                 "keeping gbk (override with --enc:sjis)\n");
            }
        }
        if (ons.init()) exit(-1);
        SDL_Log("## after ons.init()");
        ons.executeLabel();
        exit(0);
    }
}

#ifndef PSV_STATIC
int main(int argc, char *argv[])
{
    char *_argv[] = 
    {
        "app0:eboot.bin",
        "--fullscreen",
        "--fontcache",
        "--textbox",
        "--root",
        "ux0:data/onsemu/120_chs",
        "--touch-mode",
        "use_front_only_touch"
    };
    if(argc>1)
    {
        ons_main(argc, argv);
    }
    else
    {
        ons_main(sizeof(_argv)/sizeof(char*), _argv);
    }
    
}
#endif

#if defined(PSV) && defined(PSV_MODULE)
extern "C"
{
    void _start() __attribute__ ((weak, alias("module_start")));
    int module_start(SceSize args, void *argp) 
    {
        SDL_Log("onsjh module_start args=%d\n", args);
        return SCE_KERNEL_START_SUCCESS;
    }

    int module_stop(SceSize args, void *argp) 
    {
        SDL_Log("onsjh module_stop\n");
        return SCE_KERNEL_STOP_SUCCESS;
    }

    int module_exit()
    {
        SDL_Log("onsjh module_exit\n");
        return SCE_KERNEL_STOP_SUCCESS;
    }
}
#endif