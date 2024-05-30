// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;

/** Renders View hierarchies to text for debugging. */
public class ViewPrinter {

    /** Options to customize rendering and printing. */
    public static class Options {
        private static final Options DEFAULT = new Options();

        private String mLogTag = "ViewPrinter";
        private boolean mPrintNonVisibleViews;
        private boolean mPrintResourcePackage;

        public Options setLogTag(String logTag) {
            mLogTag = logTag;
            return this;
        }

        public Options setPrintNonVisibleViews(boolean printNonVisibleViews) {
            mPrintNonVisibleViews = printNonVisibleViews;
            return this;
        }

        public Options setPrintResourcePackage(boolean printResourcePackage) {
            mPrintResourcePackage = printResourcePackage;
            return this;
        }
    }

    /**
     * Print out a representation of a View hierarchy to logcat for debugging.
     *
     * @param rootView the root View to start at
     */
    public static void printView(View rootView) {
        printView(rootView, Options.DEFAULT);
    }

    /**
     * Print out a representation of a View hierarchy to logcat for debugging.
     *
     * @param rootView the root View to start at
     * @param options options for representing the View Hierarchy
     */
    public static void printView(View rootView, Options options) {
        String description = describeView(rootView, options);
        for (String line : description.split("\\n")) {
            Log.i(options.mLogTag, line);
        }
    }

    /**
     * Dump the representation of a View hierarchy to a String for debugging.
     *
     * @param rootView the root View to start at
     * @return a String representing the View hierarchy
     */
    public static String describeView(View rootView) {
        return describeView(rootView, Options.DEFAULT);
    }

    /**
     * Dump the representation of a View hierarchy to a String for debugging.
     *
     * @param rootView the root View to start at
     * @param options options for representing the View Hierarchy
     * @return a String representing the View hierarchy
     */
    public static String describeView(View rootView, Options options) {
        TreeOutput treeOutput = describeViewRecursive(rootView, options);
        if (treeOutput != null) {
            return treeOutput.render();
        } else {
            return "<root view is not visible>";
        }
    }

    private static @Nullable TreeOutput describeViewRecursive(View rootView, Options options) {
        if (!options.mPrintNonVisibleViews && rootView.getVisibility() != View.VISIBLE) {
            return null;
        }

        StringBuilder stringBuilder = new StringBuilder();

        if (rootView.getVisibility() != View.VISIBLE) {
            stringBuilder.append("~ ");
        }

        if (rootView instanceof TextView) {
            TextView v = (TextView) rootView;
            stringBuilder.append('"');
            stringBuilder.append(v.getText());
            stringBuilder.append("\" | ");
        }

        String resourceName = getViewResourceName(rootView, options);
        if (resourceName != null) {
            stringBuilder.append(resourceName);
            stringBuilder.append(" | ");
        }

        stringBuilder.append(rootView.getClass().getSimpleName());

        stringBuilder.append('\n');

        TreeOutput output = new TreeOutput(stringBuilder.toString());

        if (rootView instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) rootView;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                TreeOutput childOutput = describeViewRecursive(viewGroup.getChildAt(i), options);
                if (childOutput != null) {
                    output.addChild(childOutput);
                }
            }
        }

        return output;
    }

    private static String getViewResourceName(View v, Options options) {
        Resources resources = v.getResources();
        if (resources == null) {
            return "<no resources>";
        }

        int id = v.getId();
        if (id == View.NO_ID) {
            return null;
        }

        if (options.mPrintResourcePackage) {
            try {
                return resources.getResourceName(id);
            } catch (Resources.NotFoundException e) {
                return "<invalid id>";
            }
        } else {
            String name;
            try {
                name = resources.getResourceEntryName(id);
            } catch (Resources.NotFoundException e) {
                return "<invalid id>";
            }
            return String.format("@id/%s", name);
        }
    }

    /**
     * Tree of strings to be output with a structure that looks like this:
     *
     * <pre>
     * @id/control_container | ToolbarControlContainer
     * ├── @id/toolbar_container | ToolbarViewResourceFrameLayout
     * │   ╰── @id/toolbar | ToolbarPhone
     * │       ├── @id/home_button | HomeButton
     * │       ├── @id/location_bar | LocationBarPhone
     * │       │   ├── @id/location_bar_status | StatusView
     * │       │   │   ╰── @id/location_bar_status_icon_view | StatusIconView
     * │       │   │       ╰── @id/location_bar_status_icon_frame | FrameLayout
     * │       │   │           ╰── @id/loc_bar_status_icon | ChromeImageView
     * │       │   ╰── "about:blank" | @id/url_bar | UrlBarApi26
     * │       ╰── @id/toolbar_buttons | LinearLayout
     * │           ├── @id/tab_switcher_button | ToggleTabStackButton
     * │           ╰── @id/menu_button_wrapper | MenuButton
     * │               ╰── @id/menu_button | ChromeImageButton
     * ╰── @id/tab_switcher_toolbar | StartSurfaceToolbarView
     *     ├── @id/new_tab_view | LinearLayout
     *     │   ├── AppCompatImageView
     *     │   ╰── "New tab" | MaterialTextView
     *     ╰── @id/menu_anchor | FrameLayout
     *         ╰── @id/menu_button_wrapper | MenuButton
     *             ╰── @id/menu_button | ChromeImageButton
     * </pre>
     */
    public static class TreeOutput {
        private String mLabel;
        private @Nullable List<TreeOutput> mChildren;

        public TreeOutput(String label) {
            mLabel = label;
        }

        public void addChild(TreeOutput child) {
            Objects.requireNonNull(child);

            if (mChildren == null) {
                mChildren = new ArrayList<>();
            }
            mChildren.add(child);
        }

        public String render() {
            StringBuilder stringBuilder = new StringBuilder();
            render("", "", stringBuilder);
            return stringBuilder.toString();
        }

        private void render(
                String structureFirstLine,
                String structureOtherLines,
                StringBuilder stringBuilder) {
            stringBuilder.append(structureFirstLine);
            stringBuilder.append(mLabel);

            if (mChildren == null) {
                return;
            }

            Iterator<TreeOutput> it = mChildren.iterator();
            while (it.hasNext()) {
                TreeOutput child = it.next();
                String newStructureFirstLine;
                String newStructureOtherLines;
                if (it.hasNext()) {
                    // Not the last child
                    newStructureFirstLine = structureOtherLines + "├── ";
                    newStructureOtherLines = structureOtherLines + "│   ";
                } else {
                    // Last child
                    newStructureFirstLine = structureOtherLines + "╰── ";
                    newStructureOtherLines = structureOtherLines + "    ";
                }
                child.render(newStructureFirstLine, newStructureOtherLines, stringBuilder);
            }
        }
    }
}
