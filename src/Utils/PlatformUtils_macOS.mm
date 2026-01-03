#include "PlatformUtils.h"
#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <filesystem>
#include <vector>
#include <string>

namespace Utils {

    std::vector<FontInfo> GetSystemFonts() {
        std::vector<FontInfo> fonts;
        std::vector<std::string> searchPaths = {
            "/Library/Fonts",
            "/System/Library/Fonts",
            [NSHomeDirectory() stringByAppendingString:@"/Library/Fonts"].UTF8String
        };

        for (const auto& path : searchPaths) {
            if (std::filesystem::exists(path)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".ttf" || ext == ".otf") {
                            fonts.push_back({ entry.path().stem().string(), entry.path().string() });
                        }
                    }
                }
            }
        }
        return fonts;
    }

    std::string PickFolderDialog() {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:NO];
        [panel setCanChooseDirectories:YES];
        [panel setAllowsMultipleSelection:NO];

        if ([panel runModal] == NSModalResponseOK) {
            return [[[panel URL] path] UTF8String];
        }
        return "";
    }

    std::string PickFileDialog(const char* filter) {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];

        if ([panel runModal] == NSModalResponseOK) {
            return [[[panel URL] path] UTF8String];
        }
        return "";
    }

    std::string SaveFileDialog(const char* filter, const char* defaultName) {
        NSSavePanel* panel = [NSSavePanel savePanel];
        if (defaultName) {
            [panel setNameFieldStringValue:[NSString stringWithUTF8String:defaultName]];
        }

        if ([panel runModal] == NSModalResponseOK) {
            return [[[panel URL] path] UTF8String];
        }
        return "";
    }

    std::string GetConfigDir() {
        NSString* path = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) firstObject];
        NSString* fullPath = [path stringByAppendingPathComponent:@"FntGenerator"];
        
        NSFileManager* fileManager = [NSFileManager defaultManager];
        if (![fileManager fileExistsAtPath:fullPath]) {
            [fileManager createDirectoryAtPath:fullPath withIntermediateDirectories:YES attributes:nil error:nil];
        }
        
        return [fullPath UTF8String];
    }

    std::string GetExecutablePath() {
        NSString* path = [[NSBundle mainBundle] executablePath];
        if (path) {
            NSString* dir = [path stringByDeletingLastPathComponent];
            return [dir UTF8String];
        }
        return ".";
    }
}
#endif
