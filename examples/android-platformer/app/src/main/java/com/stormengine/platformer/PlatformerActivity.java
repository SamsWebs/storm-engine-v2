package com.stormengine.platformer;

import android.content.res.AssetManager;
import android.os.Bundle;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Extracts the APK's assets/ into internal storage on startup so the engine's
 * plain-file I/O (std::ifstream in TileMapLoader, etc.) works unchanged — the
 * native side chdir()s to SDL_AndroidGetInternalStoragePath() and reads
 * "./assets/..." exactly like the desktop build.
 */
public class PlatformerActivity extends SDLActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // The Gradle sourceSet packs the platformer's asset folders at the APK
        // assets root; recreate the "./assets/<dir>" layout the game expects.
        for (String dir : new String[] {"fonts", "gfx", "tilemaps"}) {
            copyAssetDir(dir, new File(getFilesDir(), "assets/" + dir));
        }
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "SDL2_image", "SDL2_ttf", "SDL2_mixer", "main" };
    }

    private void copyAssetDir(String srcPath, File dst) {
        AssetManager am = getAssets();
        try {
            String[] children = am.list(srcPath);
            if (children == null || children.length == 0) {
                copyAssetFile(srcPath, dst); // leaf: a file
                return;
            }
            dst.mkdirs();
            for (String child : children) {
                copyAssetDir(srcPath + "/" + child, new File(dst, child));
            }
        } catch (Exception e) {
            // Missing directories are fine; individual failures are logged by SDL.
        }
    }

    private void copyAssetFile(String srcPath, File dst) throws Exception {
        if (dst.exists() && dst.length() > 0) return; // already extracted
        try (InputStream in = getAssets().open(srcPath);
             OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[16 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
    }
}
