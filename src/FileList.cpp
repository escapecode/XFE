// File list. Taken from the FOX library and slightly modified.
// The compare(), compare_nolocale() and compare_locale() functions are adapted from a patch
// submitted by Vladimir Támara Patiño


#include "config.h"
#include "i18n.h"

#include <fx.h>
#include <fxkeys.h>
#include <FXPNGIcon.h>
#include <FXJPGIcon.h>
#include <FXTIFIcon.h>
#if defined(linux)
#include <mntent.h>
#endif

#include "xfedefs.h"
#include "icons.h"
#include "xfeutils.h"
#include "File.h"
#include "FileDict.h"
#include "IconList.h"
#include "MessageBox.h"
#include "InputDialog.h"
#include "StringList.h"
#include "FileList.h"



// Number of columns in detailed view, in the general case
#define NB_HEADERS    8

// Minimum file name size in detailed view
#ifndef MIN_NAME_SIZE
#define MIN_NAME_SIZE    75
#endif

// Time interval before refreshing the view
#define REFRESH_INTERVAL     1000
#define REFRESH_FREQUENCY    30

// Time interval before opening a folder
#define OPEN_INTERVAL        1000

// Counter limit for image refreshing
#define REFRESH_COUNT        100

#define HASH1(x, n)    (((FXuint)(x)*13)%(n))         // Probe Position [0..n-1]
#define HASH2(x, n)    (1|(((FXuint)(x)*17)%((n)-1))) // Probe Distance [1..n-1]


#if defined(linux)
FXStringDict* fsdevices = NULL; // Devices from fstab
FXStringDict* mtdevices = NULL; // Mounted devices
FXStringDict* updevices = NULL; // Responding devices
#endif

extern FXbool   allowPopupScroll;
extern FXString xdgdatahome;


// Object implementation
FXIMPLEMENT(FileItem, IconItem, NULL, 0)


// Map
FXDEFMAP(FileList) FileListMap[] =
{
    FXMAPFUNC(SEL_DRAGGED, 0, FileList::onDragged),
    FXMAPFUNC(SEL_TIMEOUT, FileList::ID_REFRESH_TIMER, FileList::onCmdRefreshTimer),
    FXMAPFUNC(SEL_TIMEOUT, FileList::ID_OPEN_TIMER, FileList::onOpenTimer),
    FXMAPFUNC(SEL_DND_ENTER, 0, FileList::onDNDEnter),
    FXMAPFUNC(SEL_DND_LEAVE, 0, FileList::onDNDLeave),
    FXMAPFUNC(SEL_DND_DROP, 0, FileList::onDNDDrop),
    FXMAPFUNC(SEL_DND_MOTION, 0, FileList::onDNDMotion),
    FXMAPFUNC(SEL_DND_REQUEST, 0, FileList::onDNDRequest),
    FXMAPFUNC(SEL_BEGINDRAG, 0, FileList::onBeginDrag),
    FXMAPFUNC(SEL_ENDDRAG, 0, FileList::onEndDrag),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_DRAG_COPY, FileList::onCmdDragCopy),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_DRAG_MOVE, FileList::onCmdDragMove),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_DRAG_LINK, FileList::onCmdDragLink),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_DRAG_REJECT, FileList::onCmdDragReject),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_DIRECTORY_UP, FileList::onCmdDirectoryUp),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_NAME, FileList::onCmdSortByName),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_DIRNAME, FileList::onCmdSortByDirName),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_TYPE, FileList::onCmdSortByType),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_SIZE, FileList::onCmdSortBySize),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_EXT, FileList::onCmdSortByExt),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_TIME, FileList::onCmdSortByTime),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_USER, FileList::onCmdSortByUser),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_GROUP, FileList::onCmdSortByGroup),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_PERM, FileList::onCmdSortByPerm),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_DELTIME, FileList::onCmdSortByDeltime),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_BY_ORIGPATH, FileList::onCmdSortByOrigpath),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_REVERSE, FileList::onCmdSortReverse),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SORT_CASE, FileList::onCmdSortCase),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_DIRS_FIRST, FileList::onCmdDirsFirst),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SET_PATTERN, FileList::onCmdSetPattern),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SHOW_HIDDEN, FileList::onCmdShowHidden),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_HIDE_HIDDEN, FileList::onCmdHideHidden),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_TOGGLE_HIDDEN, FileList::onCmdToggleHidden),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_TOGGLE_THUMBNAILS, FileList::onCmdToggleThumbnails),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_HEADER_CHANGE, FileList::onCmdHeader),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_REFRESH, FileList::onCmdRefresh),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_SHOW_FOLDERS, FileList::onCmdShowFolders),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_HIDE_FOLDERS, FileList::onCmdHideFolders),
    FXMAPFUNC(SEL_COMMAND, FileList::ID_TOGGLE_FOLDERS, FileList::onCmdToggleFolders),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_HEADER_CHANGE, FileList::onUpdHeader),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_DIRECTORY_UP, FileList::onUpdDirectoryUp),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_NAME, FileList::onUpdSortByName),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_DIRNAME, FileList::onUpdSortByDirName),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_TYPE, FileList::onUpdSortByType),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_SIZE, FileList::onUpdSortBySize),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_EXT, FileList::onUpdSortByExt),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_TIME, FileList::onUpdSortByTime),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_USER, FileList::onUpdSortByUser),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_GROUP, FileList::onUpdSortByGroup),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_PERM, FileList::onUpdSortByPerm),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_DELTIME, FileList::onUpdSortByDeltime),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_BY_ORIGPATH, FileList::onUpdSortByOrigpath),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_REVERSE, FileList::onUpdSortReverse),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SORT_CASE, FileList::onUpdSortCase),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_DIRS_FIRST, FileList::onUpdDirsFirst),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SET_PATTERN, FileList::onUpdSetPattern),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SHOW_HIDDEN, FileList::onUpdShowHidden),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_HIDE_HIDDEN, FileList::onUpdHideHidden),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_TOGGLE_HIDDEN, FileList::onUpdToggleHidden),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_TOGGLE_THUMBNAILS, FileList::onUpdToggleThumbnails),
    FXMAPFUNC(SEL_UPDATE, 0, FileList::onUpdRefreshTimer),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_SHOW_FOLDERS, FileList::onUpdShowFolders),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_HIDE_FOLDERS, FileList::onUpdHideFolders),
    FXMAPFUNC(SEL_UPDATE, FileList::ID_TOGGLE_FOLDERS, FileList::onUpdToggleFolders),
};


// Object implementation
FXIMPLEMENT(FileList, IconList, FileListMap, ARRAYNUMBER(FileListMap))



// File List
// Note : the attribute _ICONLIST_FILELIST indicates that the icon list is a file list
FileList::FileList(FXWindow* focuswin, FXComposite* p, FXObject* tgt, FXSelector sel, FXbool showthumbs, FXuint opts, int x, int y, int w, int h) :
    IconList(p, tgt, sel, opts, x, y, w, h), directory(ROOTDIR), orgdirectory(ROOTDIR), pattern("*")
{
    flags |= FLAG_ENABLED|FLAG_DROPTARGET;
    associations = NULL;
    appendHeader(_("Name"), NULL, 200);
    if (options&_FILELIST_SEARCH)
    {
        appendHeader(_("Folder"), NULL, 150);
    }
    appendHeader(_("Size"), NULL, 60);
    appendHeader(_("Type"), NULL, 100);
    appendHeader(_("Extension"), NULL, 100);
    appendHeader(_("Modified date"), NULL, 150);
    appendHeader(_("User"), NULL, 50);
    appendHeader(_("Group"), NULL, 50);
    appendHeader(_("Permissions"), NULL, 100);

    // Initializations
    matchmode = FILEMATCH_FILE_NAME|FILEMATCH_NOESCAPE;
    associations = new FileDict(getApp());
    dropaction = DRAG_MOVE;
    sortfunc = ascendingCase;
    dirsfirst = true;
    match_by_type = false;
    allowrefresh = true;
    timestamp = 0;
    counter = 1;
    prevIndex = -1;
    list = NULL;
    displaythumbnails = showthumbs;
    backhist = NULL;
    forwardhist = NULL;
    focuswindow = focuswin;
    deldatesize = 0;
    origpathsize = 0;

#if defined(linux)
    // Initialize the fsdevices, mtdevices and updevices lists
    // if it was not done in DirList (useful for XFileWrite, XFilePackage and XFileImage)
    struct mntent* mnt;
    if (fsdevices == NULL)
    {
        // To list file system devices
        fsdevices = new FXStringDict();
        FILE* fstab = setmntent(FSTAB_PATH, "r");
        if (fstab)
        {
            while ((mnt = getmntent(fstab)))
            {
                if (!streq(mnt->mnt_type, MNTTYPE_IGNORE) && !streq(mnt->mnt_type, MNTTYPE_SWAP) &&
                    !streq(mnt->mnt_dir, "/"))
                {
                    if (!strncmp(mnt->mnt_fsname, "/dev/fd", 7))
                    {
                        fsdevices->insert(mnt->mnt_dir, "floppy");
                    }
                    else if (!strncmp(mnt->mnt_type, "iso", 3))
                    {
                        fsdevices->insert(mnt->mnt_dir, "cdrom");
                    }
                    else if (!strncmp(mnt->mnt_fsname, "/dev/zip", 8))
                    {
                        fsdevices->insert(mnt->mnt_dir, "zip");
                    }
                    else if (streq(mnt->mnt_type, "nfs"))
                    {
                        fsdevices->insert(mnt->mnt_dir, "nfsdisk");
                    }
                    else if (streq(mnt->mnt_type, "smbfs"))
                    {
                        fsdevices->insert(mnt->mnt_dir, "smbdisk");
                    }
                    else
                    {
                        fsdevices->insert(mnt->mnt_dir, "harddisk");
                    }
                }
            }
            endmntent(fstab);
        }
    }
    if (mtdevices == NULL)
    {
        // To list mounted devices
        mtdevices = new FXStringDict();
        FILE* mtab = setmntent(MTAB_PATH, "r");
        if (mtab)
        {
            while ((mnt = getmntent(mtab)))
            {
                // To fix an issue with some Linux distributions
                FXString mntdir = mnt->mnt_dir;
                if ((mntdir != "/dev/.static/dev") && (mntdir.rfind("gvfs", 4, mntdir.length()) == -1))
                {
                    mtdevices->insert(mnt->mnt_dir, mnt->mnt_type);
                }
            }
            endmntent(mtab);
        }
    }
    if (updevices == NULL)
    {
        // To mark mount points that are up or down
        updevices = new FXStringDict();
        struct stat statbuf;
        FXString    mtstate;
        FILE*       mtab = setmntent(MTAB_PATH, "r");
        if (mtab)
        {
            while ((mnt = getmntent(mtab)))
            {
                // To fix an issue with some Linux distributions
                FXString mntdir = mnt->mnt_dir;
                if ((mntdir != "/dev/.static/dev") && (mntdir.rfind("gvfs", 4, mntdir.length()) == -1))
                {
                    if (lstatmt(mnt->mnt_dir, &statbuf) == -1)
                    {
                        mtstate = "down";
                    }
                    else
                    {
                        mtstate = "up";
                    }
                    updevices->insert(mnt->mnt_dir, mtstate.text());
                }
            }
            endmntent(mtab);
        }
    }
#endif

    // Trashcan location
    trashfileslocation = xdgdatahome + PATHSEPSTRING TRASHFILESPATH;
    trashinfolocation = xdgdatahome + PATHSEPSTRING TRASHINFOPATH;
}


#if defined(linux)

// Force mtdevices list refresh
void FileList::refreshMtdevices(void)
{
    struct mntent* mnt;
    FXStringDict*  tmpdict = new FXStringDict();
    FILE*          mtab = setmntent(MTAB_PATH, "r");

    if (mtab)
    {
        while ((mnt = getmntent(mtab)))
        {
            // To fix an issue with some Linux distributions
            FXString mntdir = mnt->mnt_dir;
            if ((mntdir != "/dev/.static/dev") && (mntdir.rfind("gvfs", 4, mntdir.length()) == -1))
            {
                tmpdict->insert(mnt->mnt_dir, "");
                if (mtdevices->find(mnt->mnt_dir))
                {
                    mtdevices->remove(mnt->mnt_dir);
                }
                mtdevices->insert(mnt->mnt_dir, mnt->mnt_type);
            }
        }
        endmntent(mtab);
    }

    // Remove mount points that don't exist anymore
    int s;
    for (s = mtdevices->first(); s < mtdevices->size(); s = mtdevices->next(s))
    {
        if (!tmpdict->find(mtdevices->key(s)))
        {
            mtdevices->remove(mtdevices->key(s));
        }
    }

    delete tmpdict;
}


#endif


// Create the file list
void FileList::create()
{
    IconList::create();
    if (!deleteType)
    {
        deleteType = getApp()->registerDragType(deleteTypeName);
    }
    if (!urilistType)
    {
        urilistType = getApp()->registerDragType(urilistTypeName);
    }
    getApp()->addTimeout(this, ID_REFRESH_TIMER, REFRESH_INTERVAL);
}


// Cleanup
FileList::~FileList()
{
    getApp()->removeTimeout(this, ID_REFRESH_TIMER);
    getApp()->removeTimeout(this, ID_OPEN_TIMER);
    delete associations;
    delete forwardhist;
    delete backhist;

    associations = (FileDict*)-1;
    list = (FileItem*)-1L;
}


// Open up folder when hovering long over a folder
long FileList::onOpenTimer(FXObject*, FXSelector, void*)
{
    int    xx, yy, index;
    FXuint state;

    getCursorPosition(xx, yy, state);
    index = getItemAt(xx, yy);
    if ((0 <= index) && isItemDirectory(index))
    {
        dropdirectory = getItemPathname(index);
        setDirectory(dropdirectory);
        getApp()->addTimeout(this, ID_OPEN_TIMER, OPEN_INTERVAL);
        prevIndex = -1;
    }
    return(1);
}


// Handle drag-and-drop enter
long FileList::onDNDEnter(FXObject* sender, FXSelector sel, void* ptr)
{
    IconList::onDNDEnter(sender, sel, ptr);

    // Keep original directory
    orgdirectory = getDirectory();

    return(1);
}


// Handle drag-and-drop leave
long FileList::onDNDLeave(FXObject* sender, FXSelector sel, void* ptr)
{
    IconList::onDNDLeave(sender, sel, ptr);

    if (prevIndex != -1)
    {
        setItemMiniIcon(prevIndex, minifoldericon);
        setItemBigIcon(prevIndex, bigfoldericon);
        prevIndex = -1;
    }

    // Cancel open up timer
    getApp()->removeTimeout(this, ID_OPEN_TIMER);

    // Stop scrolling
    stopAutoScroll();

    // Restore original directory
    setDirectory(orgdirectory);
    return(1);
}


// Handle drag-and-drop motion
long FileList::onDNDMotion(FXObject* sender, FXSelector sel, void* ptr)
{
    FXEvent* event = (FXEvent*)ptr;
    int      index = -1;

    // Cancel open up timer
    getApp()->removeTimeout(this, ID_OPEN_TIMER);

    // Start autoscrolling
    if (startAutoScroll(event, false))
    {
        return(1);
    }

    // Give base class a shot
    if (IconList::onDNDMotion(sender, sel, ptr))
    {
        return(1);
    }

    // Dropping list of filenames
    if (offeredDNDType(FROM_DRAGNDROP, urilistType))
    {
        index = getItemAt(event->win_x, event->win_y);
        if ((prevIndex != -1) && (prevIndex != index))
        {
            // Symlink folders have a different icon
            if (isItemLink(prevIndex))
            {
                setItemMiniIcon(prevIndex, minilinkicon);
                setItemBigIcon(prevIndex, biglinkicon);
            }
            else
            {
                setItemMiniIcon(prevIndex, minifoldericon);
                setItemBigIcon(prevIndex, bigfoldericon);
            }
            prevIndex = -1;
        }

        // Drop in the background
        dropdirectory = getDirectory();

        // What is being done (move,copy,link)
        dropaction = inquireDNDAction();

        // We will open up a folder if we can hover over it for a while
        if ((0 <= index) && isItemDirectory(index))
        {
            // Set open up timer
            getApp()->addTimeout(this, ID_OPEN_TIMER, OPEN_INTERVAL);
            prevIndex = index;
            setItemMiniIcon(index, minifolderopenicon);
            setItemBigIcon(index, bigfolderopenicon);

            // Directory where to drop, or directory to open up
            dropdirectory = getItemPathname(index);
        }

        // See if dropdirectory is writable
        if (::isWritable(dropdirectory))
        {
            acceptDrop(DRAG_ACCEPT);
        }
        return(1);
    }
    return(0);
}


// Set drag type to copy
long FileList::onCmdDragCopy(FXObject* sender, FXSelector sel, void* ptr)
{
    dropaction = DRAG_COPY;
    return(1);
}


// Set drag type to move
long FileList::onCmdDragMove(FXObject* sender, FXSelector sel, void* ptr)
{
    dropaction = DRAG_MOVE;
    return(1);
}


// Set drag type to symlink
long FileList::onCmdDragLink(FXObject* sender, FXSelector sel, void* ptr)
{
    dropaction = DRAG_LINK;
    return(1);
}


// Cancel drag action
long FileList::onCmdDragReject(FXObject* sender, FXSelector sel, void* ptr)
{
    dropaction = DRAG_REJECT;
    return(1);
}


// Handle drag-and-drop drop
long FileList::onDNDDrop(FXObject* sender, FXSelector sel, void* ptr)
{
    FXuchar* data;
    FXuint   len;
    FXbool   showdialog = true;
    File*    f = NULL;
    int      ret;

    FXbool ask_before_copy = getApp()->reg().readUnsignedEntry("OPTIONS", "ask_before_copy", true);
    FXbool confirm_dnd = getApp()->reg().readUnsignedEntry("OPTIONS", "confirm_drag_and_drop", true);


    // Cancel open up timer
    getApp()->removeTimeout(this, ID_OPEN_TIMER);

    // Stop scrolling
    stopAutoScroll();

    // Perhaps target wants to deal with it
    if (IconList::onDNDDrop(sender, sel, ptr))
    {
        return(1);
    }

    // Check if control key or shift key were pressed
    FXbool ctrlshiftkey = false;

    if (ptr != NULL)
    {
        FXEvent* event = (FXEvent*)ptr;
        if (event->state&CONTROLMASK)
        {
            ctrlshiftkey = true;
        }
        if (event->state&SHIFTMASK)
        {
            ctrlshiftkey = true;
        }
    }

    // Get DND data
    // This is done before displaying the popup menu to fix a drag and drop problem with konqueror and dolphin file managers
    FXbool dnd = getDNDData(FROM_DRAGNDROP, urilistType, data, len);

    // Display the dnd dialog if the control or shift key were not pressed
    if (confirm_dnd & !ctrlshiftkey)
    {
        // Display a popup to select the drag type
        dropaction = DRAG_REJECT;
        FXMenuPane menu(this);
        int        x, y;
        FXuint     state;
        getRoot()->getCursorPosition(x, y, state);
        new FXMenuCommand(&menu, _("Copy here"), copy_clpicon, this, FileList::ID_DRAG_COPY);
        new FXMenuCommand(&menu, _("Move here"), moveiticon, this, FileList::ID_DRAG_MOVE);
        new FXMenuCommand(&menu, _("Link here"), minilinkicon, this, FileList::ID_DRAG_LINK);
        new FXMenuSeparator(&menu);
        new FXMenuCommand(&menu, _("Cancel"), NULL, this, FileList::ID_DRAG_REJECT);
        menu.create();
        allowPopupScroll = true;  // Allow keyboard scrolling
        menu.popup(NULL, x, y);
        getApp()->runModalWhileShown(&menu);
        allowPopupScroll = false;
    }

    if (prevIndex != -1)
    {
        setItemMiniIcon(prevIndex, minifoldericon);
        setItemBigIcon(prevIndex, bigfoldericon);
        prevIndex = -1;
    }
    // Restore original directory
    setDirectory(orgdirectory);

    // Get uri-list of files being dropped
    if (dnd)  // See comment upper
    {
        FXRESIZE(&data, FXuchar, len+1);
        data[len] = '\0';
        char* p, *q;
        p = q = (char*)data;

        // Number of selected items
        FXString buf = p;
        int      num = buf.contains('\n')+1;

        // Eventually correct the number of selected items
        // because sometimes there is another '\n' at the end of the string
        int pos = buf.rfind('\n');
        if (pos == buf.length()-1)
        {
            num = num-1;
        }

        // File object
        if (dropaction == DRAG_COPY)
        {
            f = new File(this, _("File copy"), COPY, num);
        }
        else if (dropaction == DRAG_MOVE)
        {
            f = new File(this, _("File move"), MOVE, num);
        }
        else if (dropaction == DRAG_LINK)
        {
            f = new File(this, _("File symlink"), SYMLINK, num);
        }
        else
        {
            FXFREE(&data);
            return(0);
        }

        // Target directory
        FXString targetdir = dropdirectory;

        while (*p)
        {
            while (*q && *q != '\r')
            {
                q++;
            }
            FXString url(p, q-p);
            FXString source(FXURL::decode(FXURL::fileFromURL(url)));
            FXString target(targetdir);
            FXString sourcedir = FXPath::directory(source);

            // File operation dialog, if needed
            if (((!confirm_dnd) | ctrlshiftkey) & ask_before_copy & showdialog)
            {
                FXIcon*  icon = NULL;
                FXString title, message;
                if (dropaction == DRAG_COPY)
                {
                    title = _("Copy");
                    icon = copy_bigicon;
                    if (num == 1)
                    {
                        message = title+source;
                    }
                    else
                    {
                        message.format(_("Copy %s files/folders.\nFrom: %s"), FXStringVal(num).text(), sourcedir.text());
                    }
                }
                else if (dropaction == DRAG_MOVE)
                {
                    title = _("Move");
                    icon = move_bigicon;
                    if (num == 1)
                    {
                        message = title+source;
                    }
                    else
                    {
                        message.format(_("Move %s files/folders.\nFrom: %s"), FXStringVal(num).text(), sourcedir.text());
                    }
                }
                else if ((dropaction == DRAG_LINK) && (num == 1))
                {
                    title = _("Symlink");
                    icon = link_bigicon;
                    message = title+source;
                }

                InputDialog* dialog = new InputDialog(this, targetdir, message, title, _("To:"), icon);
                dialog->CursorEnd();
                int rc = 1;
                rc = dialog->execute();
                target = dialog->getText();
                target = ::filePath(target);

                if (num > 1)
                {
                    showdialog = false;
                }
                delete dialog;
                if (!rc)
                {
                    return(0);
                }
            }
            // Move the source file
            if (dropaction == DRAG_MOVE)
            {
                // Move file
                f->create();

                // If target file is located at trash location, also create the corresponding trashinfo file
                // Do it silently and don't report any error if it fails
                FXbool use_trash_can = getApp()->reg().readUnsignedEntry("OPTIONS", "use_trash_can", true);
                if (use_trash_can && (FXPath::directory(target) == trashfileslocation))
                {
                    // Trash files path name
                    FXString trashpathname = createTrashpathname(source, trashfileslocation);

                    // Adjust target name to get the _N suffix if any
                    FXString trashtarget = FXPath::directory(target)+PATHSEPSTRING+FXPath::name(trashpathname);

                    // Create trashinfo file
                    createTrashinfo(source, trashpathname, trashfileslocation, trashinfolocation);

                    // Move source to trash target
                    ret = f->move(source, trashtarget);
                }

                // Move source to target
                else
                {
                    //target=FXPath::directory(target);
                    ret = f->move(source, target);
                }

                // If source file is located at trash location, try to also remove the corresponding trashinfo file if it exists
                // Do it silently and don't report any error if it fails
                if (use_trash_can && ret && (source.left(trashfileslocation.length()) == trashfileslocation))
                {
                    FXString trashinfopathname = trashinfolocation+PATHSEPSTRING+FXPath::name(source)+".trashinfo";
                    ::unlink(trashinfopathname.text());
                }

                // An error has occurred
                if ((ret == 0) && !f->isCancelled())
                {
                    f->hideProgressDialog();
                    MessageBox::error(this, BOX_OK, _("Error"), _("An error has occurred during the move file operation!"));
                    break;
                }

                // If action is cancelled in progress dialog
                if (f->isCancelled())
                {
                    f->hideProgressDialog();
                    MessageBox::error(this, BOX_OK, _("Warning"), _("Move file operation cancelled!"));
                    break;
                }
            }
            // Copy the source file
            else if (dropaction == DRAG_COPY)
            {
                // Copy file
                f->create();

                // If target file is located at trash location, also create the corresponding trashinfo file
                // Do it silently and don't report any error if it fails
                FXbool use_trash_can = getApp()->reg().readUnsignedEntry("OPTIONS", "use_trash_can", true);
                if (use_trash_can && (FXPath::directory(target) == trashfileslocation))
                {
                    // Trash files path name
                    FXString trashpathname = createTrashpathname(source, trashfileslocation);

                    // Adjust target name to get the _N suffix if any
                    FXString trashtarget = FXPath::directory(target)+PATHSEPSTRING+FXPath::name(trashpathname);

                    // Create trashinfo file
                    createTrashinfo(source, trashpathname, trashfileslocation, trashinfolocation);

                    // Copy source to trash target
                    ret = f->copy(source, trashtarget);
                }

                // Copy source to target
                else
                {
                    //target=FXPath::directory(target);
                    ret = f->copy(source, target);
                }

                // An error has occurred
                if ((ret == 0) && !f->isCancelled())
                {
                    f->hideProgressDialog();
                    MessageBox::error(this, BOX_OK, _("Error"), _("An error has occurred during the copy file operation!"));
                    break;
                }

                // If action is cancelled in progress dialog
                if (f->isCancelled())
                {
                    f->hideProgressDialog();
                    MessageBox::error(this, BOX_OK, _("Warning"), _("Copy file operation cancelled!"));
                    break;
                }
            }
            // Link the source file (no progress dialog in this case)
            else if (dropaction == DRAG_LINK)
            {
                // Link file
                f->create();
                f->symlink(source, target);
            }
            if (*q == '\r')
            {
                q += 2;
            }
            p = q;
        }
        delete f;
        FXFREE(&data);
        return(1);
    }

    return(0);
}


// Somebody wants our dragged data
long FileList::onDNDRequest(FXObject* sender, FXSelector sel, void* ptr)
{
    FXEvent* event = (FXEvent*)ptr;
    FXuchar* data;
    FXuint   len;

    // Perhaps the target wants to supply its own data
    if (IconList::onDNDRequest(sender, sel, ptr))
    {
        return(1);
    }

    // Return list of filenames as a uri-list
    if (event->target == urilistType)
    {
        if (!dragfiles.empty())
        {
            len = dragfiles.length();
            FXMEMDUP(&data, dragfiles.text(), FXuchar, len);
            setDNDData(FROM_DRAGNDROP, event->target, data, len);
        }
        return(1);
    }

    // Delete selected files
    if (event->target == deleteType)
    {
        return(1);
    }

    return(0);
}


// Start a drag operation
long FileList::onBeginDrag(FXObject* sender, FXSelector sel, void* ptr)
{
    register int i;

    if (IconList::onBeginDrag(sender, sel, ptr))
    {
        return(1);
    }
    if (beginDrag(&urilistType, 1))
    {
        dragfiles = FXString::null;
        for (i = 0; i < getNumItems(); i++)
        {
            if (isItemSelected(i))
            {
                if (getItemFilename(i) == "..")
                {
                    deselectItem(i);
                }
                else
                {
                    dragfiles += FXURL::encode(::fileToURI(getItemFullPathname(i)));
                    dragfiles += "\r\n";
                }
            }
        }
        return(1);
    }
    return(0);
}


// End drag operation
long FileList::onEndDrag(FXObject* sender, FXSelector sel, void* ptr)
{
    if (IconList::onEndDrag(sender, sel, ptr))
    {
        return(1);
    }
    endDrag((didAccept() != DRAG_REJECT));
    setDragCursor(getDefaultCursor());
    dragfiles = FXString::null;
    return(1);
}


// Dragged stuff around
long FileList::onDragged(FXObject* sender, FXSelector sel, void* ptr)
{
    FXEvent*     event = (FXEvent*)ptr;
    FXDragAction action;

    if (IconList::onDragged(sender, sel, ptr))
    {
        return(1);
    }
    action = DRAG_MOVE;

    if (event->state&CONTROLMASK)
    {
        action = DRAG_COPY;
    }
    if (event->state&SHIFTMASK)
    {
        action = DRAG_MOVE;
    }
    if ((event->state&CONTROLMASK) && (event->state&SHIFTMASK))
    {
        action = DRAG_LINK;
    }

    // If source directory is read-only, convert move action to copy
    if (!::isWritable(orgdirectory) && (action == DRAG_MOVE))
    {
        action = DRAG_COPY;
    }

    handleDrag(event->root_x, event->root_y, action);
    if (didAccept() != DRAG_REJECT)
    {
        if (action == DRAG_MOVE)
        {
            setDragCursor(getApp()->getDefaultCursor(DEF_DNDMOVE_CURSOR));
        }
        else if (action == DRAG_LINK)
        {
            setDragCursor(getApp()->getDefaultCursor(DEF_DNDLINK_CURSOR));
        }
        else
        {
            setDragCursor(getApp()->getDefaultCursor(DEF_DNDCOPY_CURSOR));
        }
    }
    else
    {
        setDragCursor(getApp()->getDefaultCursor(DEF_DNDSTOP_CURSOR));
    }
    return(1);
}


// Toggle hidden files display
long FileList::onCmdToggleHidden(FXObject*, FXSelector, void*)
{
    showHiddenFiles(!shownHiddenFiles());
    return(1);
}

// Toggle hidden folders display
long FileList::onCmdToggleFolders(FXObject*, FXSelector, void*)
{
    showHiddenFiles(!hiddenFolders());
    return(1);
}

// Toggle thumbnails display
long FileList::onCmdToggleThumbnails(FXObject*, FXSelector, void*)
{
    showThumbnails(!displaythumbnails);
    return(1);
}


// Update toggle thumbnails button
long FileList::onUpdToggleThumbnails(FXObject* sender, FXSelector, void*)
{
    if (shownThumbnails())
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), NULL);
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    }
    return(1);
}


// Update toggle hidden files button
long FileList::onUpdToggleHidden(FXObject* sender, FXSelector, void*)
{
    if (shownHiddenFiles())
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), NULL);
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    }
    return(1);
}


// Show hidden files
long FileList::onCmdShowHidden(FXObject*, FXSelector, void*)
{
    showHiddenFiles(true);
    return(1);
}


// Update show hidden files widget
long FileList::onUpdShowHidden(FXObject* sender, FXSelector, void*)
{
    if (shownHiddenFiles())
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), NULL);
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    }
    return(1);
}


// Hide hidden files
long FileList::onCmdHideHidden(FXObject*, FXSelector, void*)
{
    showHiddenFiles(false);
    return(1);
}


// Update hide hidden files widget
long FileList::onUpdHideHidden(FXObject* sender, FXSelector, void*)
{
    if (!shownHiddenFiles())
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), NULL);
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    }
    return(1);
}

// Update toggle hidden folders button
long FileList::onUpdToggleFolders(FXObject* sender, FXSelector, void*)
{
    if (hiddenFolders())
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), NULL);
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    }
    return(1);
}

// Show hidden files
long FileList::onCmdShowFolders(FXObject*, FXSelector, void*)
{
    showFolders(false);
    return(1);
}


// Update show hidden files widget
long FileList::onUpdShowFolders(FXObject* sender, FXSelector, void*)
{
    if (hiddenFolders())
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), NULL);
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    }
    return(1);
}


// Hide hidden files
long FileList::onCmdHideFolders(FXObject*, FXSelector, void*)
{
	// the code below is a hack because onCmdToggleFolders is not being triggered
    if (! hiddenFolders())
    {
		showFolders(true);
        //sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), NULL);
    }
    else
    {
		showFolders(false);
        //sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    }
    return(1);
}


// Update hide hidden files widget
long FileList::onUpdHideFolders(FXObject* sender, FXSelector, void*)
{
	//printf("onUpdHideFolders %i\n", hiddenFolders());
    if (!hiddenFolders())
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), NULL);
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    }
    return(1);
}

// Move up one level
long FileList::onCmdDirectoryUp(FXObject*, FXSelector, void*)
{
    setFocus();
    setDirectory(FXPath::upLevel(directory), true, directory);
    return(1);
}


// Determine if we can still go up more
long FileList::onUpdDirectoryUp(FXObject* sender, FXSelector, void* ptr)
{
    FXuint msg = FXPath::isTopDirectory(directory) ? ID_DISABLE : ID_ENABLE;

    sender->handle(this, FXSEL(SEL_COMMAND, msg), ptr);
    return(1);
}


// Change pattern
long FileList::onCmdSetPattern(FXObject*, FXSelector, void* ptr)
{
    if (!ptr)
    {
        return(0);
    }
    setPattern((const char*)ptr);
    return(1);
}


// Update pattern
long FileList::onUpdSetPattern(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, FXSEL(SEL_COMMAND, FXWindow::ID_SETVALUE), (void*)pattern.text());
    return(1);
}


// Sort by name
long FileList::onCmdSortByName(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        if (getIgnoreCase() == true)
        {
            sortfunc = (sortfunc == ascendingCaseMix) ? descendingCaseMix : ascendingCaseMix;
        }
        else
        {
            sortfunc = (sortfunc == ascendingMix) ? descendingMix : ascendingMix;
        }
    }
    else
    {
        if (getIgnoreCase() == true)
        {
            sortfunc = (sortfunc == ascendingCase) ? descendingCase : ascendingCase;
        }
        else
        {
            sortfunc = (sortfunc == ascending) ? descending : ascending;
        }
    }
    setSortHeader(0);
    scan(true);
    return(1);
}


// Update sender
long FileList::onUpdSortByName(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascending || sortfunc == descending || sortfunc == ascendingCase || sortfunc == descendingCase ||
                          sortfunc == ascendingMix || sortfunc == descendingMix || sortfunc == ascendingCaseMix || sortfunc == descendingCaseMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    return(1);
}


// Sort by directory name (for search list)
long FileList::onCmdSortByDirName(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        if (getIgnoreCase() == true)
        {
            sortfunc = (sortfunc == ascendingDirCaseMix) ? descendingDirCaseMix : ascendingDirCaseMix;
        }
        else
        {
            sortfunc = (sortfunc == ascendingDirMix) ? descendingDirMix : ascendingDirMix;
        }
    }
    else
    {
        if (getIgnoreCase() == true)
        {
            sortfunc = (sortfunc == ascendingDirCase) ? descendingDirCase : ascendingDirCase;
        }
        else
        {
            sortfunc = (sortfunc == ascendingDir) ? descendingDir : ascendingDir;
        }
    }
    scan(true);
    return(1);
}


// Update sender
long FileList::onUpdSortByDirName(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascendingDir || sortfunc == descendingDir || sortfunc == ascendingDirCase || sortfunc == descendingDirCase ||
                          sortfunc == ascendingDirMix || sortfunc == descendingDirMix || sortfunc == ascendingDirCaseMix || sortfunc == descendingDirCaseMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    return(1);
}


// Sort by type
long FileList::onCmdSortByType(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        sortfunc = (sortfunc == ascendingTypeMix) ? descendingTypeMix : ascendingTypeMix;
    }
    else
    {
        sortfunc = (sortfunc == ascendingType) ? descendingType : ascendingType;
    }

    if (options&_FILELIST_SEARCH)
    {
        setSortHeader(3);
    }
    else
    {
        setSortHeader(2);
    }

    scan(true);
    return(1);
}


// Update sender
long FileList::onUpdSortByType(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascendingType || sortfunc == descendingType || sortfunc == ascendingTypeMix || sortfunc == descendingTypeMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    return(1);
}


// Sort by size
long FileList::onCmdSortBySize(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        sortfunc = (sortfunc == ascendingSizeMix) ? descendingSizeMix : ascendingSizeMix;
    }
    else
    {
        sortfunc = (sortfunc == ascendingSize) ? descendingSize : ascendingSize;
    }

    if (options&_FILELIST_SEARCH)
    {
        setSortHeader(2);
    }
    else
    {
        setSortHeader(1);
    }

    scan(true);
    return(1);
}


// Update sender
long FileList::onUpdSortBySize(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascendingSize || sortfunc == descendingSize || sortfunc == ascendingSizeMix || sortfunc == descendingSizeMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    return(1);
}


// Sort by ext
long FileList::onCmdSortByExt(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        sortfunc = (sortfunc == ascendingExtMix) ? descendingExtMix : ascendingExtMix;
    }
    else
    {
        sortfunc = (sortfunc == ascendingExt) ? descendingExt : ascendingExt;
    }

    if (options&_FILELIST_SEARCH)
    {
        setSortHeader(4);
    }
    else
    {
        setSortHeader(3);
    }

    scan(true);
    return(1);
}


// Sort by original path
long FileList::onCmdSortByOrigpath(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        sortfunc = (sortfunc == ascendingOrigpathMix) ? descendingOrigpathMix : ascendingOrigpathMix;
    }
    else
    {
        sortfunc = (sortfunc == ascendingOrigpath) ? descendingOrigpath : ascendingOrigpath;
    }

    setSortHeader(8);

    scan(true);
    return(1);
}


// Sort by deletion time
long FileList::onCmdSortByDeltime(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        sortfunc = (sortfunc == ascendingDeltimeMix) ? descendingDeltimeMix : ascendingDeltimeMix;
    }
    else
    {
        sortfunc = (sortfunc == ascendingDeltime) ? descendingDeltime : ascendingDeltime;
    }

    setSortHeader(9);

    scan(true);
    return(1);
}


// Update sender
long FileList::onUpdSortByExt(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascendingExt || sortfunc == descendingExt || sortfunc == ascendingExtMix || sortfunc == descendingExtMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    return(1);
}


// Sort by time
long FileList::onCmdSortByTime(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        sortfunc = (sortfunc == ascendingTimeMix) ? descendingTimeMix : ascendingTimeMix;
    }
    else
    {
        sortfunc = (sortfunc == ascendingTime) ? descendingTime : ascendingTime;
    }

    if (options&_FILELIST_SEARCH)
    {
        setSortHeader(5);
    }
    else
    {
        setSortHeader(4);
    }

    scan(true);
    return(1);
}


// Update sender
long FileList::onUpdSortByTime(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascendingTime || sortfunc == descendingTime || sortfunc == ascendingTimeMix || sortfunc == descendingTimeMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    return(1);
}


// Sort by user
long FileList::onCmdSortByUser(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        sortfunc = (sortfunc == ascendingUserMix) ? descendingUserMix : ascendingUserMix;
    }
    else
    {
        sortfunc = (sortfunc == ascendingUser) ? descendingUser : ascendingUser;
    }

    if (options&_FILELIST_SEARCH)
    {
        setSortHeader(6);
    }
    else
    {
        setSortHeader(5);
    }

    scan(true);
    return(1);
}


// Update sender
long FileList::onUpdSortByUser(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascendingUser || sortfunc == descendingUser || sortfunc == ascendingUserMix || sortfunc == descendingUserMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    return(1);
}


// Sort by group
long FileList::onCmdSortByGroup(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        sortfunc = (sortfunc == ascendingGroupMix) ? descendingGroupMix : ascendingGroupMix;
    }
    else
    {
        sortfunc = (sortfunc == ascendingGroup) ? descendingGroup : ascendingGroup;
    }

    if (options&_FILELIST_SEARCH)
    {
        setSortHeader(7);
    }
    else
    {
        setSortHeader(6);
    }

    scan(true);
    return(1);
}


// Update sender
long FileList::onUpdSortByGroup(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascendingGroup || sortfunc == descendingGroup || sortfunc == ascendingGroupMix || sortfunc == descendingGroupMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    return(1);
}


// Sort by permissions
long FileList::onCmdSortByPerm(FXObject*, FXSelector, void*)
{
    if (dirsfirst == false)
    {
        sortfunc = (sortfunc == ascendingPermMix) ? descendingPermMix : ascendingPermMix;
    }
    else
    {
        sortfunc = (sortfunc == ascendingPerm) ? descendingPerm : ascendingPerm;
    }

    if (options&_FILELIST_SEARCH)
    {
        setSortHeader(8);
    }
    else
    {
        setSortHeader(7);
    }

    scan(true);
    return(1);
}


// Update sender
long FileList::onUpdSortByPerm(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascendingPerm || sortfunc == descendingPerm || sortfunc == ascendingPermMix || sortfunc == descendingPermMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);
    return(1);
}


// Update sender
long FileList::onUpdSortByDeltime(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascendingDeltime || sortfunc == descendingDeltime || sortfunc == ascendingDeltimeMix || sortfunc == descendingDeltimeMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);

    if (!(options&_FILELIST_SEARCH) && (getNumHeaders() == NB_HEADERS+2))
    {
        sender->handle(this, FXSEL(SEL_COMMAND, FXWindow::ID_ENABLE), NULL);
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, FXWindow::ID_DISABLE), NULL);
    }

    return(1);
}


// Update sender
long FileList::onUpdSortByOrigpath(FXObject* sender, FXSelector, void*)
{
    sender->handle(this, (sortfunc == ascendingOrigpath || sortfunc == descendingOrigpath || sortfunc == ascendingOrigpathMix || sortfunc == descendingOrigpathMix) ? FXSEL(SEL_COMMAND, ID_CHECK) : FXSEL(SEL_COMMAND, ID_UNCHECK), NULL);

    if (!(options&_FILELIST_SEARCH) && (getNumHeaders() == NB_HEADERS+2))
    {
        sender->handle(this, FXSEL(SEL_COMMAND, FXWindow::ID_ENABLE), NULL);
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, FXWindow::ID_DISABLE), NULL);
    }

    return(1);
}


// Reverse sort order
long FileList::onCmdSortReverse(FXObject*, FXSelector, void*)
{
    if (sortfunc == ascending)
    {
        sortfunc = descending;
    }

    else if (sortfunc == ascendingMix)
    {
        sortfunc = descendingMix;
    }

    else if (sortfunc == descending)
    {
        sortfunc = ascending;
    }

    else if (sortfunc == descendingMix)
    {
        sortfunc = ascendingMix;
    }

    else if (sortfunc == ascendingCase)
    {
        sortfunc = descendingCase;
    }

    else if (sortfunc == ascendingCaseMix)
    {
        sortfunc = descendingCaseMix;
    }

    else if (sortfunc == descendingCase)
    {
        sortfunc = ascendingCase;
    }

    else if (sortfunc == descendingCaseMix)
    {
        sortfunc = ascendingCaseMix;
    }

    else if (sortfunc == ascendingType)
    {
        sortfunc = descendingType;
    }

    else if (sortfunc == ascendingTypeMix)
    {
        sortfunc = descendingTypeMix;
    }

    else if (sortfunc == descendingType)
    {
        sortfunc = ascendingType;
    }

    else if (sortfunc == descendingTypeMix)
    {
        sortfunc = ascendingTypeMix;
    }

    else if (sortfunc == ascendingExt)
    {
        sortfunc = descendingExt;
    }

    else if (sortfunc == ascendingExtMix)
    {
        sortfunc = descendingExtMix;
    }

    else if (sortfunc == descendingExt)
    {
        sortfunc = ascendingExt;
    }

    else if (sortfunc == descendingExtMix)
    {
        sortfunc = ascendingExtMix;
    }

    else if (sortfunc == ascendingSize)
    {
        sortfunc = descendingSize;
    }

    else if (sortfunc == ascendingSizeMix)
    {
        sortfunc = descendingSizeMix;
    }

    else if (sortfunc == descendingSize)
    {
        sortfunc = ascendingSize;
    }

    else if (sortfunc == descendingSizeMix)
    {
        sortfunc = ascendingSizeMix;
    }

    else if (sortfunc == ascendingTime)
    {
        sortfunc = descendingTime;
    }

    else if (sortfunc == ascendingTimeMix)
    {
        sortfunc = descendingTimeMix;
    }

    else if (sortfunc == descendingTime)
    {
        sortfunc = ascendingTime;
    }

    else if (sortfunc == descendingTimeMix)
    {
        sortfunc = ascendingTimeMix;
    }

    else if (sortfunc == ascendingUserMix)
    {
        sortfunc = descendingUser;
    }

    else if (sortfunc == ascendingUserMix)
    {
        sortfunc = descendingUser;
    }

    else if (sortfunc == descendingUser)
    {
        sortfunc = ascendingUser;
    }

    else if (sortfunc == descendingUserMix)
    {
        sortfunc = ascendingUserMix;
    }

    else if (sortfunc == ascendingGroup)
    {
        sortfunc = descendingGroup;
    }

    else if (sortfunc == ascendingGroupMix)
    {
        sortfunc = descendingGroupMix;
    }

    else if (sortfunc == descendingGroup)
    {
        sortfunc = ascendingGroup;
    }

    else if (sortfunc == descendingGroupMix)
    {
        sortfunc = ascendingGroupMix;
    }

    else if (sortfunc == ascendingPerm)
    {
        sortfunc = descendingPerm;
    }

    else if (sortfunc == ascendingPermMix)
    {
        sortfunc = descendingPermMix;
    }

    else if (sortfunc == descendingPerm)
    {
        sortfunc = ascendingPerm;
    }

    else if (sortfunc == descendingPermMix)
    {
        sortfunc = ascendingPermMix;
    }

    else if (sortfunc == ascendingDir)
    {
        sortfunc = descendingDir;
    }

    else if (sortfunc == ascendingDirMix)
    {
        sortfunc = descendingDirMix;
    }

    else if (sortfunc == descendingDir)
    {
        sortfunc = ascendingDir;
    }

    else if (sortfunc == descendingDirMix)
    {
        sortfunc = ascendingDirMix;
    }

    else if (sortfunc == ascendingDirCase)
    {
        sortfunc = descendingDirCase;
    }

    else if (sortfunc == ascendingDirCaseMix)
    {
        sortfunc = descendingDirCaseMix;
    }

    else if (sortfunc == descendingDirCase)
    {
        sortfunc = ascendingDirCase;
    }

    else if (sortfunc == descendingDirCaseMix)
    {
        sortfunc = ascendingDirCaseMix;
    }

    else if (sortfunc == ascendingOrigpath)
    {
        sortfunc = descendingOrigpath;
    }

    else if (sortfunc == ascendingOrigpathMix)
    {
        sortfunc = descendingOrigpathMix;
    }

    else if (sortfunc == descendingOrigpath)
    {
        sortfunc = ascendingOrigpath;
    }

    else if (sortfunc == descendingOrigpathMix)
    {
        sortfunc = ascendingOrigpathMix;
    }

    else if (sortfunc == ascendingDeltime)
    {
        sortfunc = descendingDeltime;
    }

    else if (sortfunc == ascendingDeltimeMix)
    {
        sortfunc = descendingDeltimeMix;
    }

    else if (sortfunc == descendingDeltime)
    {
        sortfunc = ascendingDeltime;
    }

    else if (sortfunc == descendingDeltimeMix)
    {
        sortfunc = ascendingDeltimeMix;
    }

    scan(true);
    return(1);
}


// Update sender
long FileList::onUpdSortReverse(FXObject* sender, FXSelector, void*)
{
    FXSelector selector = FXSEL(SEL_COMMAND, ID_UNCHECK);

    if (sortfunc == descending)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingCase)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingCaseMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingType)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingTypeMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingExt)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingExtMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingSize)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingSizeMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingTime)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingTimeMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingUser)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingUserMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingGroup)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingGroupMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingPerm)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingPermMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingDir)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingDirMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingDirCase)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingDirCaseMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingOrigpath)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingOrigpathMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingDeltime)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    else if (sortfunc == descendingDeltimeMix)
    {
        selector = FXSEL(SEL_COMMAND, ID_CHECK);
    }

    sender->handle(this, selector, NULL);

    return(1);
}


// Toggle case sensitivity
long FileList::onCmdSortCase(FXObject*, FXSelector, void*)
{
    setIgnoreCase(!getIgnoreCase());
    if (dirsfirst == false)
    {
        if (getIgnoreCase() == true)
        {
            if ((sortfunc == ascending) || (sortfunc == ascendingMix) || (sortfunc == ascendingCase))
            {
                sortfunc = ascendingCaseMix;
            }

            else if ((sortfunc == descending) || (sortfunc == descendingMix) || (sortfunc == descendingCase))
            {
                sortfunc = descendingCaseMix;
            }

            else if ((sortfunc == ascendingDir) || (sortfunc == ascendingDirMix) || (sortfunc == ascendingDirCase))
            {
                sortfunc = ascendingDirCaseMix;
            }

            else if ((sortfunc == descendingDir) || (sortfunc == descendingDirMix) || (sortfunc == descendingDirCase))
            {
                sortfunc = descendingDirCaseMix;
            }
        }
        else
        {
            if ((sortfunc == ascending) || (sortfunc == ascendingCase) || (sortfunc == ascendingCaseMix))
            {
                sortfunc = ascendingMix;
            }

            else if ((sortfunc == descending) || (sortfunc == descendingCase) || (sortfunc == descendingCaseMix))
            {
                sortfunc = descendingMix;
            }

            else if ((sortfunc == ascendingDir) || (sortfunc == ascendingDirCase) || (sortfunc == ascendingDirCaseMix))
            {
                sortfunc = ascendingDirMix;
            }

            else if ((sortfunc == descendingDir) || (sortfunc == descendingDirCase) || (sortfunc == descendingDirCaseMix))
            {
                sortfunc = descendingDirMix;
            }
        }
    }
    else
    {
        if (getIgnoreCase() == true)
        {
            if ((sortfunc == ascending) || (sortfunc == ascendingMix) || (sortfunc == ascendingCaseMix))
            {
                sortfunc = ascendingCase;
            }

            else if ((sortfunc == descending) || (sortfunc == descendingMix) || (sortfunc == descendingCaseMix))
            {
                sortfunc = descendingCase;
            }

            else if ((sortfunc == ascendingDir) || (sortfunc == ascendingDirMix) || (sortfunc == ascendingDirCaseMix))
            {
                sortfunc = ascendingDirCase;
            }

            else if ((sortfunc == descendingDir) || (sortfunc == descendingDirMix) || (sortfunc == descendingDirCaseMix))
            {
                sortfunc = descendingDirCase;
            }
        }
        else
        {
            if ((sortfunc == ascendingMix) || (sortfunc == ascendingCase) || (sortfunc == ascendingCaseMix))
            {
                sortfunc = ascending;
            }

            else if ((sortfunc == descendingMix) || (sortfunc == descendingCase) || (sortfunc == descendingCaseMix))
            {
                sortfunc = descending;
            }

            else if ((sortfunc == ascendingDirMix) || (sortfunc == ascendingDirCase) || (sortfunc == ascendingDirCaseMix))
            {
                sortfunc = ascendingDir;
            }

            else if ((sortfunc == descendingDirMix) || (sortfunc == descendingDirCase) || (sortfunc == descendingDirCaseMix))
            {
                sortfunc = descendingDir;
            }
        }
    }
    scan(true);
    return(1);
}


// Update case sensitivity
long FileList::onUpdSortCase(FXObject* sender, FXSelector, void* ptr)
{
    if ((sortfunc == ascending) || (sortfunc == descending) || (sortfunc == ascendingMix) || (sortfunc == descendingMix) ||
        (sortfunc == ascendingCase) || (sortfunc == descendingCase) || (sortfunc == ascendingCaseMix) || (sortfunc == descendingCaseMix))
    {
        sender->handle(this, FXSEL(SEL_COMMAND, FXWindow::ID_ENABLE), ptr);

        if (getIgnoreCase() == true)
        {
            sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), ptr);
        }
        else
        {
            sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), ptr);
        }
    }
    else if ((sortfunc == ascendingDir) || (sortfunc == descendingDir) || (sortfunc == ascendingDirMix) || (sortfunc == descendingDirMix) ||
             (sortfunc == ascendingDirCase) || (sortfunc == descendingDirCase) || (sortfunc == ascendingDirCaseMix) || (sortfunc == descendingDirCaseMix))
    {
        sender->handle(this, FXSEL(SEL_COMMAND, FXWindow::ID_ENABLE), ptr);

        if (getIgnoreCase() == true)
        {
            sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), ptr);
        }
        else
        {
            sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), ptr);
        }
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, FXWindow::ID_DISABLE), ptr);
    }

    return(1);
}


// Toggle directories first
long FileList::onCmdDirsFirst(FXObject*, FXSelector, void*)
{
    dirsfirst = !dirsfirst;
    if (dirsfirst == false)
    {
        if (sortfunc == ascending)
        {
            sortfunc = ascendingMix;
        }

        else if (sortfunc == descending)
        {
            sortfunc = descendingMix;
        }

        else if (sortfunc == ascendingCase)
        {
            sortfunc = ascendingCaseMix;
        }

        else if (sortfunc == descendingCase)
        {
            sortfunc = descendingCaseMix;
        }

        else if (sortfunc == ascendingType)
        {
            sortfunc = ascendingTypeMix;
        }

        else if (sortfunc == descendingType)
        {
            sortfunc = descendingTypeMix;
        }

        else if (sortfunc == ascendingExt)
        {
            sortfunc = ascendingExtMix;
        }

        else if (sortfunc == descendingExt)
        {
            sortfunc = descendingExtMix;
        }

        else if (sortfunc == ascendingSize)
        {
            sortfunc = ascendingSizeMix;
        }

        else if (sortfunc == descendingSize)
        {
            sortfunc = descendingSizeMix;
        }

        else if (sortfunc == ascendingTime)
        {
            sortfunc = ascendingTimeMix;
        }

        else if (sortfunc == descendingTime)
        {
            sortfunc = descendingTimeMix;
        }

        else if (sortfunc == ascendingUser)
        {
            sortfunc = ascendingUserMix;
        }

        else if (sortfunc == descendingUser)
        {
            sortfunc = descendingUserMix;
        }

        else if (sortfunc == ascendingGroup)
        {
            sortfunc = ascendingGroupMix;
        }

        else if (sortfunc == descendingGroup)
        {
            sortfunc = descendingGroupMix;
        }

        else if (sortfunc == ascendingPerm)
        {
            sortfunc = ascendingPermMix;
        }

        else if (sortfunc == descendingPerm)
        {
            sortfunc = descendingPermMix;
        }

        else if (sortfunc == ascendingDir)
        {
            sortfunc = ascendingDirMix;
        }

        else if (sortfunc == descendingDir)
        {
            sortfunc = descendingDirMix;
        }

        else if (sortfunc == ascendingDirCase)
        {
            sortfunc = ascendingDirCaseMix;
        }

        else if (sortfunc == descendingDirCase)
        {
            sortfunc = descendingDirCaseMix;
        }

        else if (sortfunc == ascendingOrigpath)
        {
            sortfunc = ascendingOrigpathMix;
        }

        else if (sortfunc == descendingOrigpath)
        {
            sortfunc = descendingOrigpathMix;
        }

        else if (sortfunc == ascendingDeltime)
        {
            sortfunc = ascendingDeltimeMix;
        }

        else if (sortfunc == descendingDeltime)
        {
            sortfunc = descendingDeltimeMix;
        }
    }
    else
    {
        if (sortfunc == ascendingMix)
        {
            sortfunc = ascending;
        }

        else if (sortfunc == descendingMix)
        {
            sortfunc = descending;
        }

        else if (sortfunc == ascendingCaseMix)
        {
            sortfunc = ascendingCase;
        }

        else if (sortfunc == descendingCaseMix)
        {
            sortfunc = descendingCase;
        }

        else if (sortfunc == ascendingTypeMix)
        {
            sortfunc = ascendingType;
        }

        else if (sortfunc == descendingTypeMix)
        {
            sortfunc = descendingType;
        }

        else if (sortfunc == ascendingExtMix)
        {
            sortfunc = ascendingExt;
        }

        else if (sortfunc == descendingExtMix)
        {
            sortfunc = descendingExt;
        }

        else if (sortfunc == ascendingSizeMix)
        {
            sortfunc = ascendingSize;
        }

        else if (sortfunc == descendingSizeMix)
        {
            sortfunc = descendingSize;
        }

        else if (sortfunc == ascendingTimeMix)
        {
            sortfunc = ascendingTime;
        }

        else if (sortfunc == descendingTimeMix)
        {
            sortfunc = descendingTime;
        }

        else if (sortfunc == ascendingUserMix)
        {
            sortfunc = ascendingUser;
        }

        else if (sortfunc == descendingUserMix)
        {
            sortfunc = descendingUser;
        }

        else if (sortfunc == ascendingGroupMix)
        {
            sortfunc = ascendingGroup;
        }

        else if (sortfunc == descendingGroupMix)
        {
            sortfunc = descendingGroup;
        }

        else if (sortfunc == ascendingPermMix)
        {
            sortfunc = ascendingPerm;
        }

        else if (sortfunc == descendingPermMix)
        {
            sortfunc = descendingPerm;
        }

        else if (sortfunc == ascendingDirMix)
        {
            sortfunc = ascendingDir;
        }

        else if (sortfunc == descendingDirMix)
        {
            sortfunc = descendingDir;
        }

        else if (sortfunc == ascendingDirCaseMix)
        {
            sortfunc = ascendingDirCase;
        }

        else if (sortfunc == descendingDirCaseMix)
        {
            sortfunc = descendingDirCase;
        }

        else if (sortfunc == ascendingOrigpathMix)
        {
            sortfunc = ascendingOrigpath;
        }

        else if (sortfunc == descendingOrigpathMix)
        {
            sortfunc = descendingOrigpath;
        }

        else if (sortfunc == ascendingDeltimeMix)
        {
            sortfunc = ascendingDeltime;
        }

        else if (sortfunc == descendingDeltimeMix)
        {
            sortfunc = descendingDeltime;
        }
    }
    scan(true);
    return(1);
}


// Update directories first
long FileList::onUpdDirsFirst(FXObject* sender, FXSelector, void* ptr)
{
    if (dirsfirst == true)
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_CHECK), ptr);
    }
    else
    {
        sender->handle(this, FXSEL(SEL_COMMAND, ID_UNCHECK), ptr);
    }

    return(1);
}


// Clicked header button
long FileList::onCmdHeader(FXObject*, FXSelector, void* ptr)
{
    // List is a search list (a directory name column is inserted between name and file size)
    if (options&_FILELIST_SEARCH)
    {
        FXuint num = (FXuint)(FXuval)ptr;
        if (num < NB_HEADERS+1)
        {
            if (num == 0)
            {
                handle(this, FXSEL(SEL_COMMAND, ID_SORT_BY_NAME), NULL);
            }
            else if (num == 1)
            {
                handle(this, FXSEL(SEL_COMMAND, ID_SORT_BY_DIRNAME), NULL);
            }
            else
            {
                handle(this, FXSEL(SEL_COMMAND, (ID_SORT_BY_NAME+num-1)), NULL);
            }

            setSortHeader(num);
        }
        if (getHeaderSize(0) < MIN_NAME_SIZE)
        {
            setHeaderSize(0, MIN_NAME_SIZE);
        }
    }

    // List is a file list
    else
    {
        FXuint num = (FXuint)(FXuval)ptr;

        // Deletion date and original path columns are displayed
        if (getNumHeaders() == NB_HEADERS+2)
        {
            if (num < NB_HEADERS+2)
            {
                handle(this, FXSEL(SEL_COMMAND, (ID_SORT_BY_NAME+num)), NULL);
                setSortHeader(num);
            }
        }

        // Standard case
        else
        {
            if (num < NB_HEADERS)
            {
                handle(this, FXSEL(SEL_COMMAND, (ID_SORT_BY_NAME+num)), NULL);
                setSortHeader(num);
            }
        }
        if (getHeaderSize(0) < MIN_NAME_SIZE)
        {
            setHeaderSize(0, MIN_NAME_SIZE);
        }
    }
    return(1);
}


// Update header button
long FileList::onUpdHeader(FXObject*, FXSelector, void*)
{
    // List is a search list (a directory name column is inserted between name and file size)
    if (options&_FILELIST_SEARCH)
    {
        header->setArrowDir(0, (sortfunc == ascending || sortfunc == ascendingCase ||
                                sortfunc == ascendingMix || sortfunc == ascendingCaseMix)  ? false : (sortfunc == descending || sortfunc == descendingCase ||
                                                                                                      sortfunc == descendingMix || sortfunc == descendingCaseMix) ? true : MAYBE);         // Name

        header->setArrowDir(1, (sortfunc == ascendingDir || sortfunc == ascendingDirCase ||
                                sortfunc == ascendingDirMix || sortfunc == ascendingDirCaseMix)  ? false : (sortfunc == descendingDir || sortfunc == descendingDirCase ||
                                                                                                            sortfunc == descendingDirMix || sortfunc == descendingDirCaseMix) ? true : MAYBE); // Name

        header->setArrowDir(2, (sortfunc == ascendingSize || sortfunc == ascendingSizeMix)  ? false : (sortfunc == descendingSize || sortfunc == descendingSizeMix) ? true : MAYBE);           // Size
        header->setArrowDir(3, (sortfunc == ascendingType || sortfunc == ascendingTypeMix)  ? false : (sortfunc == descendingType || sortfunc == descendingTypeMix) ? true : MAYBE);           // Type
        header->setArrowDir(4, (sortfunc == ascendingExt || sortfunc == ascendingExtMix)   ? false : (sortfunc == descendingExt || sortfunc == descendingExtMix)  ? true : MAYBE);             // Extension
        header->setArrowDir(5, (sortfunc == ascendingTime || sortfunc == ascendingTimeMix)  ? false : (sortfunc == descendingTime || sortfunc == descendingTimeMix) ? true : MAYBE);           // Date
        header->setArrowDir(6, (sortfunc == ascendingUser || sortfunc == ascendingUserMix)  ? false : (sortfunc == descendingUser || sortfunc == descendingUserMix) ? true : MAYBE);           // User
        header->setArrowDir(7, (sortfunc == ascendingGroup || sortfunc == ascendingGroupMix) ? false : (sortfunc == descendingGroup || sortfunc == descendingGroupMix) ? true : MAYBE);        // Group
        header->setArrowDir(8, (sortfunc == ascendingPerm || sortfunc == ascendingPermMix)  ? false : (sortfunc == descendingPerm || sortfunc == descendingPermMix) ? true : MAYBE);           // Permissions
        if (getHeaderSize(0) < MIN_NAME_SIZE)
        {
            setHeaderSize(0, MIN_NAME_SIZE);
        }

        if ((sortfunc == ascending) || (sortfunc == ascendingCase) || (sortfunc == ascendingMix) || (sortfunc == ascendingCaseMix) || (sortfunc == descending) || (sortfunc == descendingCase) || (sortfunc == descendingMix) || (sortfunc == descendingCaseMix))
        {
            setSortHeader(0);
        }
        else if ((sortfunc == ascendingDir) || (sortfunc == ascendingDirCase) || (sortfunc == ascendingDirMix) || (sortfunc == ascendingDirCaseMix) || (sortfunc == descendingDir) || (sortfunc == descendingDirCase) || (sortfunc == descendingDirMix) || (sortfunc == descendingDirCaseMix))
        {
            setSortHeader(1);
        }
        else if ((sortfunc == ascendingSize) || (sortfunc == ascendingSizeMix) || (sortfunc == descendingSize) || (sortfunc == descendingSizeMix))
        {
            setSortHeader(2);
        }
        else if ((sortfunc == ascendingType) || (sortfunc == ascendingTypeMix) || (sortfunc == descendingType) || (sortfunc == descendingTypeMix))
        {
            setSortHeader(3);
        }
        else if ((sortfunc == ascendingExt) || (sortfunc == ascendingExtMix) || (sortfunc == descendingExt) || (sortfunc == descendingExtMix))
        {
            setSortHeader(4);
        }
        else if ((sortfunc == ascendingTime) || (sortfunc == ascendingTimeMix) || (sortfunc == descendingTime) || (sortfunc == descendingTimeMix))
        {
            setSortHeader(5);
        }
        else if ((sortfunc == ascendingUser) || (sortfunc == ascendingUserMix) || (sortfunc == descendingUser) || (sortfunc == descendingUserMix))
        {
            setSortHeader(6);
        }
        else if ((sortfunc == ascendingGroup) || (sortfunc == ascendingGroupMix) || (sortfunc == descendingGroup) || (sortfunc == descendingGroupMix))
        {
            setSortHeader(7);
        }
        else if ((sortfunc == ascendingPerm) || (sortfunc == ascendingPermMix) || (sortfunc == descendingPerm) || (sortfunc == descendingPermMix))
        {
            setSortHeader(8);
        }
    }

    // List is a file list
    else
    {
        header->setArrowDir(0, (sortfunc == ascending || sortfunc == ascendingCase ||
                                sortfunc == ascendingMix || sortfunc == ascendingCaseMix)  ? false : (sortfunc == descending || sortfunc == descendingCase ||
                                                                                                      sortfunc == descendingMix || sortfunc == descendingCaseMix) ? true : MAYBE);      // Name
        header->setArrowDir(1, (sortfunc == ascendingSize || sortfunc == ascendingSizeMix)  ? false : (sortfunc == descendingSize || sortfunc == descendingSizeMix) ? true : MAYBE);    // Size
        header->setArrowDir(2, (sortfunc == ascendingType || sortfunc == ascendingTypeMix)  ? false : (sortfunc == descendingType || sortfunc == descendingTypeMix) ? true : MAYBE);    // Type
        header->setArrowDir(3, (sortfunc == ascendingExt || sortfunc == ascendingExtMix)   ? false : (sortfunc == descendingExt || sortfunc == descendingExtMix)  ? true : MAYBE);      // Extension
        header->setArrowDir(4, (sortfunc == ascendingTime || sortfunc == ascendingTimeMix)  ? false : (sortfunc == descendingTime || sortfunc == descendingTimeMix) ? true : MAYBE);    // Date
        header->setArrowDir(5, (sortfunc == ascendingUser || sortfunc == ascendingUserMix)  ? false : (sortfunc == descendingUser || sortfunc == descendingUserMix) ? true : MAYBE);    // User
        header->setArrowDir(6, (sortfunc == ascendingGroup || sortfunc == ascendingGroupMix) ? false : (sortfunc == descendingGroup || sortfunc == descendingGroupMix) ? true : MAYBE); // Group
        header->setArrowDir(7, (sortfunc == ascendingPerm || sortfunc == ascendingPermMix)  ? false : (sortfunc == descendingPerm || sortfunc == descendingPermMix) ? true : MAYBE);    // Permissions
        if (getNumHeaders() == NB_HEADERS+2)
        {
            header->setArrowDir(8, (sortfunc == ascendingOrigpath || sortfunc == ascendingOrigpathMix) ? false : (sortfunc == descendingOrigpath || sortfunc == descendingOrigpathMix) ? true : MAYBE);   // Original path
            origpathsize = header->getItemSize(NB_HEADERS);

            header->setArrowDir(9, (sortfunc == ascendingDeltime || sortfunc == ascendingDeltimeMix) ? false : (sortfunc == descendingDeltime || sortfunc == descendingDeltimeMix) ? true : MAYBE);   // Deletion date
            deldatesize = header->getItemSize(NB_HEADERS+1);
        }
        if (getHeaderSize(0) < MIN_NAME_SIZE)
        {
            setHeaderSize(0, MIN_NAME_SIZE);
        }

        if ((sortfunc == ascending) || (sortfunc == ascendingCase) || (sortfunc == ascendingMix) || (sortfunc == ascendingCaseMix) || (sortfunc == descending) || (sortfunc == descendingCase) || (sortfunc == descendingMix) || (sortfunc == descendingCaseMix))
        {
            setSortHeader(0);
        }
        else if ((sortfunc == ascendingSize) || (sortfunc == ascendingSizeMix) || (sortfunc == descendingSize) || (sortfunc == descendingSizeMix))
        {
            setSortHeader(1);
        }
        else if ((sortfunc == ascendingType) || (sortfunc == ascendingTypeMix) || (sortfunc == descendingType) || (sortfunc == descendingTypeMix))
        {
            setSortHeader(2);
        }
        else if ((sortfunc == ascendingExt) || (sortfunc == ascendingExtMix) || (sortfunc == descendingExt) || (sortfunc == descendingExtMix))
        {
            setSortHeader(3);
        }
        else if ((sortfunc == ascendingTime) || (sortfunc == ascendingTimeMix) || (sortfunc == descendingTime) || (sortfunc == descendingTimeMix))
        {
            setSortHeader(4);
        }
        else if ((sortfunc == ascendingUser) || (sortfunc == ascendingUserMix) || (sortfunc == descendingUser) || (sortfunc == descendingUserMix))
        {
            setSortHeader(5);
        }
        else if ((sortfunc == ascendingGroup) || (sortfunc == ascendingGroupMix) || (sortfunc == descendingGroup) || (sortfunc == descendingGroupMix))
        {
            setSortHeader(6);
        }
        else if ((sortfunc == ascendingPerm) || (sortfunc == ascendingPermMix) || (sortfunc == descendingPerm) || (sortfunc == descendingPermMix))
        {
            setSortHeader(7);
        }
        else if ((sortfunc == ascendingOrigpath) || (sortfunc == ascendingOrigpathMix) || (sortfunc == descendingOrigpath) || (sortfunc == descendingOrigpathMix))
        {
            setSortHeader(8);
        }
        else if ((sortfunc == ascendingDeltime) || (sortfunc == ascendingDeltimeMix) || (sortfunc == descendingDeltime) || (sortfunc == descendingDeltimeMix))
        {
            setSortHeader(9);
        }
    }
    return(1);
}


/**
 * Compares fields of p and q, supposing they are single byte strings
 * without using the current locale.
 * @param  igncase	Ignore upper/lower-case?
 * @param  asc		Ascending?  If false is descending order
 * @param  jmp		Field to compare (separated with \t)
 *
 * @return 0 if equal, negative if p<q, positive if p>q
 * If jmp has an invalid value returns 0 and errno will be EINVAL
 */
static inline int compare_nolocale(char* p, char* q, FXbool igncase, FXbool asc, FXuint jmp)
{
    int retnames, ret = 0, i;

    // Compare names

    register char* pp = p;
    register char* qq = q;

    // Go to next '\t' or '\0'
    while (*pp != '\0' && *pp > '\t')
    {
        pp++;
    }

    while (*qq != '\0' && *qq > '\t')
    {
        qq++;
    }

    // Save characters at current position
    register char pw = *pp;
    register char qw = *qq;

    // Set characters to null, to stop comparison
    *pp = '\0';
    *qq = '\0';

    // Compare strings
    retnames = comparenat(p, q, igncase);

    // Restore saved characters
    *pp = pw;
    *qq = qw;

    if (jmp == 0)
    {
        ret = retnames;
        goto end;
    }

    // Compare other fields
    // Find where to start comparison
    if (jmp == 1)
    {
        for (i = 1; *pp && i; i -= (*pp++ == '\t'))
        {
        }
        for (i = 1; *qq && i; i -= (*qq++ == '\t'))
        {
        }
    }

    else if ((jmp > 1) && (jmp < 8))
    {
        // 2->type, 3->ext, 5->user, 6->group, 7->perm,
        // Adjust the header index depending on the list type
        if (pp[1] == '/')
        {
            for (i = jmp + 1; *pp && i; i -= (*pp++ == '\t'))
            {
            }
            for (i = jmp + 1; *qq && i; i -= (*qq++ == '\t'))
            {
            }
        }
        else
        {
            for (i = jmp; *pp && i; i -= (*pp++ == '\t'))
            {
            }
            for (i = jmp; *qq && i; i -= (*qq++ == '\t'))
            {
            }
        }
    }
    else if (jmp == 8)
    {
        for (i = 8; *pp && i; i -= (*pp++ == '\t'))
        {
        }
        for (i = 8; *qq && i; i -= (*qq++ == '\t'))
        {
        }
    }
    else
    {
        errno = EINVAL;
        return(0);
    }

    // This part between brackets to make the compiler happy!
    {
        register char* sp = pp;
        register char* sq = qq;

        // Find where to stop comparison
        while (*pp != '\0' && *pp > '\t')
        {
            pp++;
        }

        while (*qq != '\0' && *qq > '\t')
        {
            qq++;
        }

        // Save characters at current position
        pw = *pp;
        qw = *qq;

        // Set characters to null, to stop comparison
        *pp = '\0';
        *qq = '\0';

        // Compare wide strings
        ret = comparenat(sp, sq, igncase);

        // Restore saved characters
        *pp = pw;
        *qq = qw;

        if (ret == 0)
        {
            ret = retnames;
        }
    }

end:

    // If descending flip
    if (!asc)
    {
        ret = ret * -1;
    }

    return(ret);
}


/**
 * Compares fields of p and q, supposing they are wide strings
 * and using the current locale.
 * @param  igncase	Ignore upper/lower-case?
 * @param  asc		Ascending?  If false is descending order
 * @param  jmp		Field to compare (separated with \t)
 *
 * @return 0 if equal, negative if p<q, positive if p>q
 * If jmp has an invalid value returns 0 and errno will be EINVAL
 */
static inline int compare_locale(wchar_t* p, wchar_t* q, FXbool igncase, FXbool asc, int jmp)
{
    int retnames, ret = 0, i;

    // Compare names

    register wchar_t* pp = p;
    register wchar_t* qq = q;

    // Go to next '\t' or '\0'
    while (*pp != '\0' && *pp > '\t')
    {
        pp++;
    }

    while (*qq != '\0' && *qq > '\t')
    {
        qq++;
    }

    // Save characters at current position
    register wchar_t pw = *pp;
    register wchar_t qw = *qq;

    // Set characters to null, to stop comparison
    *pp = '\0';
    *qq = '\0';

    // Compare wide strings
    retnames = comparewnat(p, q, igncase);

    // Restore saved characters
    *pp = pw;
    *qq = qw;

    if (jmp == 0)
    {
        ret = retnames;
        goto end;
    }

    // Compare other fields
    // Find where to start comparison
    if (jmp == 1)
    {
        for (i = 1; *pp && i; i -= (*pp++ == '\t'))
        {
        }
        for (i = 1; *qq && i; i -= (*qq++ == '\t'))
        {
        }
    }

    else if ((jmp > 1) && (jmp < 8))
    {
        // 2->type, 3->ext, 5->user, 6->group, 7->perm,
        // Adjust the header index depending on the list type
        if (pp[1] == '/')
        {
            for (i = jmp + 1; *pp && i; i -= (*pp++ == '\t'))
            {
            }
            for (i = jmp + 1; *qq && i; i -= (*qq++ == '\t'))
            {
            }
        }
        else
        {
            for (i = jmp; *pp && i; i -= (*pp++ == '\t'))
            {
            }
            for (i = jmp; *qq && i; i -= (*qq++ == '\t'))
            {
            }
        }
    }

    else if (jmp == 8)
    {
        for (i = 8; *pp && i; i -= (*pp++ == '\t'))
        {
        }
        for (i = 8; *qq && i; i -= (*qq++ == '\t'))
        {
        }
    }
    else
    {
        errno = EINVAL;
        return(0);
    }

    // This part between brackets to make the compiler happy!
    {
        register wchar_t* sp = pp;
        register wchar_t* sq = qq;

        // Find where to stop comparison
        while (*pp != '\0' && *pp > '\t')
        {
            pp++;
        }

        while (*qq != '\0' && *qq > '\t')
        {
            qq++;
        }

        // Save characters at current position
        pw = *pp;
        qw = *qq;

        // Set characters to null, to stop comparison
        *pp = '\0';
        *qq = '\0';

        // Compare wide strings
        ret = comparewnat(sp, sq, igncase);

        // Restore saved characters
        *pp = pw;
        *qq = qw;

        if (ret == 0)
        {
            ret = retnames;
        }
    }

end:

    // If descending flip
    if (!asc)
    {
        ret = ret * -1;
    }

    return(ret);
}


/**
 * Compares a field of pa with the same field of pb, if the fields are
 * equal compare by name
 * @param  igncase	Ignore upper/lower-case?
 * @param  asc		Ascending?  If false is descending order
 * @param  mixdir	Mix directories with files?
 * @param  jmp		Field to compare (separated with \t)
 *
 * @return 0 if equal, negative if pa<pb, positive if pa>pb
 * Requires to allocate some space, if there is no memory this
 * function returns 0 and errno will be ENOMEM
 * If jmp has an invalid value returns 0 and errno will be EINVAL
 */
int FileList::compare(const IconItem* pa, const IconItem* pb,
                      FXbool igncase, FXbool asc, FXbool mixdir, FXuint jmp)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;
    register char*           p = (char*)a->label.text();
    register char*           q = (char*)b->label.text();

    // Common cases
    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }

    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    if (!mixdir)
    {
        int diff = (int)b->isDirectory() - (int)a->isDirectory();
        if (diff)
        {
            return(diff);
        }
    }

    // Prepare wide char strings
    wchar_t* wa = NULL;
    wchar_t* wb = NULL;
    size_t   an, bn;
    an = mbstowcs(NULL, (const char*)p, 0);
    if (an == (size_t)-1)
    {
        return(compare_nolocale(p, q, igncase, asc, jmp)); // If error, fall back to no locale comparison
    }
    wa = (wchar_t*)calloc(an + 1, sizeof(wchar_t));
    if (wa == NULL)
    {
        errno = ENOMEM;
        return(0);
    }
    mbstowcs(wa, p, an + 1);
    bn = mbstowcs(NULL, (const char*)q, 0);
    if (bn == (size_t)-1)
    {
        free(wa);
        return(compare_nolocale(p, q, igncase, asc, jmp)); // If error, fall back to no locale comparison
    }
    wb = (wchar_t*)calloc(bn + 1, sizeof(wchar_t));
    if (wb == NULL)
    {
        errno = ENOMEM;
        free(wa);
        free(wb);
        return(0);
    }
    mbstowcs(wb, q, bn + 1);

    // Perform comparison based on the current locale
    int ret = compare_locale(wa, wb, igncase, asc, jmp);

    // Free memory
    if (wa != NULL)
    {
        free(wa);
    }
    if (wb != NULL)
    {
        free(wb);
    }

    return(ret);
}


// Compare file names
int FileList::ascending(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, false, 0));
}


// Compare file names, mixing files and directories
int FileList::ascendingMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, true, 0));
}


// Compare file names, case insensitive
int FileList::ascendingCase(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, true, true, false, 0));
}


// Compare file names, case insensitive, mixing files and directories
int FileList::ascendingCaseMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, true, true, true, 0));
}


// Compare directory names
int FileList::ascendingDir(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, false, 1));
}


// Compare directory names, mixing files and directories
int FileList::ascendingDirMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, true, 1));
}


// Compare directory names, case insensitive
int FileList::ascendingDirCase(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, true, true, false, 1));
}


// Compare directory names, case insensitive, mixing files and directories
int FileList::ascendingDirCaseMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, true, true, true, 1));
}


// Compare file types
int FileList::ascendingType(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, false, 2));
}


// Compare file types, mixing files and directories
int FileList::ascendingTypeMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, true, 2));
}


// Compare file extension
int FileList::ascendingExt(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, false, 3));
}


// Compare file extension, mixing files and directories
int FileList::ascendingExtMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, true, 3));
}


// Compare file size - Warning: only returns the sign of the comparison!!!
int FileList::ascendingSize(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    register int diff = (int)b->isDirectory() - (int)a->isDirectory();
    register int sum = (int)b->isDirectory() + (int)a->isDirectory();

    if (diff)
    {
        return(diff);
    }
    if (sum == 2)
    {
        return(ascendingCase(pa, pb));
    }

    register FXlong l = a->size - b->size;
    if (l)
    {
        if (l >= 0)
        {
            return(1);
        }
        else
        {
            return(-1);
        }
    }
    return(ascendingCase(pa, pb));
}


// Compare file size - Warning : only returns the sign of the comparison!!!
// Mixing files and directories
int FileList::ascendingSizeMix(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();
    register int            adir = (int)a->isDirectory();
    register int            bdir = (int)b->isDirectory();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    if (adir && bdir)
    {
        return(ascendingCaseMix(pa, pb));
    }
    if (adir && !bdir)
    {
        return(-1);
    }
    if (!adir && bdir)
    {
        return(1);
    }

    register FXlong l = a->size - b->size;
    if (l)
    {
        if (l >= 0)
        {
            return(1);
        }
        else
        {
            return(-1);
        }
    }
    return(ascendingCaseMix(pa, pb));
}


// Compare file time
int FileList::ascendingTime(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    register int diff = (int)b->isDirectory() - (int)a->isDirectory();
    if (diff)
    {
        return(diff);
    }

    register FXlong l = (FXlong)a->date - (FXlong)b->date;
    if (l)
    {
        return(l);
    }
    return(ascendingCase(pa, pb));
}


// Compare file time, mixing files and directories
int FileList::ascendingTimeMix(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    register FXlong l = (FXlong)a->date - (FXlong)b->date;
    if (l)
    {
        return(l);
    }
    return(ascendingCaseMix(pa, pb));
}


// Compare file user
int FileList::ascendingUser(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, false, 5));
}


// Compare file user, mixing files and directories
int FileList::ascendingUserMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, true, 5));
}


// Compare file group
int FileList::ascendingGroup(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, false, 6));
}


// Compare file group, mixing files and directories
int FileList::ascendingGroupMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, true, 6));
}


// Compare file permissions
int FileList::ascendingPerm(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, false, 7));
}


// Compare file permissions, mixing files and directories
int FileList::ascendingPermMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, true, 7));
}


// Compare file deletion time
int FileList::ascendingDeltime(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    register int diff = (int)b->isDirectory() - (int)a->isDirectory();
    if (diff)
    {
        return(diff);
    }
    register FXlong l = (FXlong)a->deldate - (FXlong)b->deldate;
    if (l)
    {
        return(l);
    }
    return(ascendingCase(pa, pb));
}


// Compare file deletion time, mixing files and directories
int FileList::ascendingDeltimeMix(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    register FXlong l = (FXlong)a->deldate - (FXlong)b->deldate;
    if (l)
    {
        return(l);
    }
    return(ascendingCaseMix(pa, pb));
}


// Compare original path
int FileList::ascendingOrigpath(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, false, 8));
}


// Compare original path, mixing files and directories
int FileList::ascendingOrigpathMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, true, true, 8));
}


// Reversed compare file name, case insensitive
int FileList::descendingCase(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, true, false, false, 0));
}


// Reversed compare file name, case insensitive, mixing files and directories
int FileList::descendingCaseMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, true, false, true, 0));
}


// Reversed compare file name
int FileList::descending(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, false, 0));
}


// Reversed compare file name, mixing files and directories
int FileList::descendingMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, true, 0));
}


// Reversed compare directory names, case insensitive
int FileList::descendingDirCase(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, true, false, false, 1));
}


// Reversed compare directory names, case insensitive, mixing files and directories
int FileList::descendingDirCaseMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, true, false, true, 1));
}


// Reversed compare directory names
int FileList::descendingDir(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, false, 1));
}


// Reversed compare directory names, mixing files and directories
int FileList::descendingDirMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, true, 1));
}


// Reversed compare file type
int FileList::descendingType(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, false, 2));
}


// Reversed compare file type, mixing files and directories
int FileList::descendingTypeMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, true, 2));
}


// Reversed compare file extension
int FileList::descendingExt(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, false, 3));
}


// Reversed compare file extension, mixing files and directories
int FileList::descendingExtMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, true, 3));
}


// Reversed compare file size
int FileList::descendingSize(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    register int diff = (int)b->isDirectory() - (int)a->isDirectory();
    register int sum = (int)b->isDirectory() + (int)a->isDirectory();
    if (diff)
    {
        return(diff);
    }
    if (sum == 2)
    {
        return(-ascendingCase(pa, pb));
    }
    register FXlong l = a->size - b->size;
    if (l)
    {
        if (l >= 0)
        {
            return(-1);
        }
        else
        {
            return(1);
        }
    }
    return(-ascendingCase(pa, pb));
}


// Reversed compare file size, mixing files and directories
int FileList::descendingSizeMix(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();
    register int            adir = (int)a->isDirectory();
    register int            bdir = (int)b->isDirectory();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    if (adir && bdir)
    {
        return(-ascendingCaseMix(pa, pb));
    }
    if (adir && !bdir)
    {
        return(1);
    }
    if (!adir && bdir)
    {
        return(-1);
    }

    register FXlong l = a->size - b->size;
    if (l)
    {
        if (l >= 0)
        {
            return(-1);
        }
        else
        {
            return(1);
        }
    }
    return(-ascendingCaseMix(pa, pb));
}


// Reversed compare file time
int FileList::descendingTime(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    register int diff = (int)b->isDirectory() - (int)a->isDirectory();
    if (diff)
    {
        return(diff);
    }
    register FXlong l = (FXlong)a->date - (FXlong)b->date;
    if (l)
    {
        return(-l);
    }
    return(-ascendingCase(pa, pb));
}


// Reversed compare file time, mixing files and directories
int FileList::descendingTimeMix(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    register FXlong l = (FXlong)a->date - (FXlong)b->date;
    if (l)
    {
        return(-l);
    }
    return(-ascendingCaseMix(pa, pb));
}


// Reversed compare file user
int FileList::descendingUser(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, false, 5));
}


// Reversed compare file user, mixing files and directories
int FileList::descendingUserMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, true, 5));
}


// Reversed compare file group
int FileList::descendingGroup(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, false, 6));
}


// Reversed compare file group, mixing files and directories
int FileList::descendingGroupMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, true, 6));
}


// Reversed compare file permission
int FileList::descendingPerm(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, false, 7));
}


// Reversed compare file permission, mixing files and directories
int FileList::descendingPermMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, true, 7));
}


// Reversed compare file deletion time
int FileList::descendingDeltime(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    register int diff = (int)b->isDirectory() - (int)a->isDirectory();
    if (diff)
    {
        return(diff);
    }
    register FXlong l = (FXlong)a->deldate - (FXlong)b->deldate;
    if (l)
    {
        return(-l);
    }
    return(-ascendingCase(pa, pb));
}


// Reversed compare file deletion time, mixing files and directories
int FileList::descendingDeltimeMix(const IconItem* pa, const IconItem* pb)
{
    register const FileItem* a = (FileItem*)pa;
    register const FileItem* b = (FileItem*)pb;

    register const FXuchar* p = (const FXuchar*)a->label.text();
    register const FXuchar* q = (const FXuchar*)b->label.text();

    // Directory '..' should always be on top
    if ((p[0] == '.') && (p[1] == '.') && (p[2] == '\t'))
    {
        return(-1);
    }
    if ((q[0] == '.') && (q[1] == '.') && (q[2] == '\t'))
    {
        return(1);
    }

    register FXlong l = (FXlong)a->deldate - (FXlong)b->deldate;
    if (l)
    {
        return(-l);
    }
    return(-ascendingCaseMix(pa, pb));
}


// Reversed compare original path
int FileList::descendingOrigpath(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, false, 8));
}


// Reversed original path, mixing files and directories
int FileList::descendingOrigpathMix(const IconItem* pa, const IconItem* pb)
{
    return(compare(pa, pb, false, false, true, 8));
}


// Force an immediate update of the list
long FileList::onCmdRefresh(FXObject*, FXSelector, void*)
{
    // Force a refresh of the file association table
    if (associations)
    {
        delete associations;
        associations = new FileDict(getApp());
    }
    allowrefresh = true;
    scan(true);
    return(1);
}


// Allow or forbid file list refresh
void FileList::setAllowRefresh(const FXbool allow)
{
    if (allow == false)
    {
        allowrefresh = false;
    }
    else
    {
        allowrefresh = true;
    }
}


// Refresh; don't update if user is interacting with the list
long FileList::onCmdRefreshTimer(FXObject*, FXSelector, void*)
{
    // Don't refresh if window is minimized
    if (((FXTopWindow*)focuswindow)->isMinimized())
    {
        return(0);
    }

    // Don't refresh if not allowed
    if (flags&FLAG_UPDATE && allowrefresh)
    {
        scan(false);
        counter = (counter+1)%REFRESH_FREQUENCY;
    }

    // Reset timer again
    getApp()->addTimeout(this, ID_REFRESH_TIMER, REFRESH_INTERVAL);
    return(0);
}


// Set current filename
void FileList::setCurrentFile(const FXString& pathname)
{
    // FIXME notify argument?
    if (!pathname.empty())
    {
        setDirectory(FXPath::directory(pathname));
        setCurrentItem(findItem(FXPath::name(pathname)));
    }
}


// Get pathname to current file, if any
FXString FileList::getCurrentFile() const
{
    if (current < 0)
    {
        return(FXString::null);
    }
    return(getItemPathname(current));
}


// Set directory being displayed and update the history if necessary
void FileList::setDirectory(const FXString& pathname, const FXbool histupdate, FXString prevpath)
{
    // Only update the history if it was requested
    if (histupdate)
    {
        // At first call, allocate the history
        if (forwardhist == NULL)
        {
            forwardhist = new StringList();
        }
        if (backhist == NULL)
        {
            backhist = new StringList();
        }

        // Update the history
        else
        {
            backhist->insertFirstItem(getDirectory());
            forwardhist->removeAllItems();
        }
    }

    FXString path("");

    // FIXME notify argument?
    if (!pathname.empty())
    {
        path = FXPath::absolute(directory, pathname);

        while (!FXPath::isTopDirectory(path) && !::isDirectory(path))
        {
            path = FXPath::upLevel(path);
        }
        if (directory != path)
        {
            directory = ::cleanPath(path);
            clearItems();
            list = NULL;
            scan(true);
        }
    }
    // If possible, select directory we came from, otherwise select first item
    if (hasFocus())
    {
        int sel_index = 0;

        if (!prevpath.empty())
        {
            if (path == FXPath::upLevel(prevpath)) // Did we come from subdirectory?
            {
                // Find dir in list
                sel_index = findItem(FXPath::name(prevpath));
                if ((sel_index == -1) && getNumItems())
                {
                    // Not found, select first item
                    sel_index = 0;
                }
            }
        }

        if ((sel_index != -1) && getNumItems())
        {
            enableItem(sel_index);
            setCurrentItem(sel_index);
        }
    }
}

void FileList::setFilterType(FXbool use_type)
{
	match_by_type = use_type;
}

// Set the pattern to filter
void FileList::setPattern(const FXString& ptrn)
{
    if (ptrn.empty())
    {
        return;
    }
    if (pattern != ptrn)
    {
        pattern = ptrn;
        scan(true);
    }
    setFocus();
}


// Change file match mode
void FileList::setMatchMode(FXuint mode)
{
    if (matchmode != mode)
    {
        matchmode = mode;
        scan(true);
    }
    setFocus();
}


// Return true if showing hidden files
FXbool FileList::shownHiddenFiles() const
{
    return((options&_FILELIST_SHOWHIDDEN) != 0);
}


// Change show hidden files mode
void FileList::showHiddenFiles(FXbool shown)
{
    FXuint opts = shown ? (options|_FILELIST_SHOWHIDDEN) : (options&~_FILELIST_SHOWHIDDEN);

    if (opts != options)
    {
        options = opts;
        scan(true);
    }
    setFocus();
}

void FileList::showFolders(FXbool shown)
{
    FXuint opts = shown ? (options|_FILELIST_SHOWFOLDERS) : (options&~_FILELIST_SHOWFOLDERS);

    if (opts != options)
    {
        options = opts;
        scan(true);
    }
    setFocus();
}

// Return true if showing hidden files
FXbool FileList::hiddenFolders() const
{
    return((options&_FILELIST_SHOWFOLDERS) != 0);
}

// Return true if showing thumbnails
FXbool FileList::shownThumbnails() const
{
    return(displaythumbnails);
}


// Change show thumbnails mode
void FileList::showThumbnails(FXbool display)
{
    displaythumbnails = display;

    // Refresh to display or hide thumbnails
    scan(true);
    setFocus();
}


// Return true if showing directories only
FXbool FileList::showOnlyDirectories() const
{
    return((options&_FILELIST_SHOWDIRS) != 0);
}


// Change show directories only mode
void FileList::showOnlyDirectories(FXbool shown)
{
    FXuint opts = shown ? (options|_FILELIST_SHOWDIRS) : (options&~_FILELIST_SHOWDIRS);

    if (opts != options)
    {
        options = opts;
        scan(true);
    }
    setFocus();
}


// Compare till '\t' or '\0'
static FXbool fileequal(const FXString& a, const FXString& b)
{
    register const FXuchar* p1 = (const FXuchar*)a.text();
    register const FXuchar* p2 = (const FXuchar*)b.text();
    register int            c1, c2;

    do
    {
        c1 = *p1++;
        c2 = *p2++;
    } while (c1 != '\0' && c1 != '\t' && c1 == c2);
    return((c1 == '\0' || c1 == '\t') && (c2 == '\0' || c2 == '\t'));
}


// Create custom item
IconItem* FileList::createItem(const FXString& text, FXIcon* big, FXIcon* mini, void* ptr)
{
    return(new FileItem(text, big, mini, ptr));
}


// Is directory
FXbool FileList::isItemDirectory(int index) const
{
    if ((FXuint)index >= (FXuint)items.no())
    {
        fprintf(stderr, "%s::isItemDirectory: index out of range.\n", getClassName());
        exit(EXIT_FAILURE);
    }
    return((((FileItem*)items[index])->state&FileItem::FOLDER) != 0);
}


// Is file
FXbool FileList::isItemFile(int index) const
{
    if ((FXuint)index >= (FXuint)items.no())
    {
        fprintf(stderr, "%s::isItemFile: index out of range.\n", getClassName());
        exit(EXIT_FAILURE);
    }
    return((((FileItem*)items[index])->state&(FileItem::FOLDER|FileItem::CHARDEV|FileItem::BLOCKDEV|FileItem::FIFO|FileItem::SOCK)) == 0);
}


// Is executable
FXbool FileList::isItemExecutable(int index) const
{
    if ((FXuint)index >= (FXuint)items.no())
    {
        fprintf(stderr, "%s::isItemExecutable: index out of range.\n", getClassName());
        exit(EXIT_FAILURE);
    }
    return((((FileItem*)items[index])->state&FileItem::EXECUTABLE) != 0);
}


// Is link
FXbool FileList::isItemLink(int index) const
{
    if ((FXuint)index >= (FXuint)items.no())
    {
        fprintf(stderr, "%s::isItemLink: index out of range.\n", getClassName());
        exit(EXIT_FAILURE);
    }
    return((((FileItem*)items[index])->state&FileItem::SYMLINK) != 0);
}


// Get file name from item at index
FXString FileList::getItemFilename(int index) const
{
    if ((FXuint)index >= (FXuint)items.no())
    {
        fprintf(stderr, "%s::getItemFilename: index out of range.\n", getClassName());
        exit(EXIT_FAILURE);
    }
    FXString label = items[index]->getText();
    return(label.section('\t', 0));
}


// Get pathname from item at index, relatively to the current directory
FXString FileList::getItemPathname(int index) const
{
    if ((FXuint)index >= (FXuint)items.no())
    {
        fprintf(stderr, "%s::getItemPathname: index out of range.\n", getClassName());
        exit(EXIT_FAILURE);
    }
    FXString label = items[index]->getText();
    return(FXPath::absolute(directory, label.section('\t', 0)));
}


// Get full pathname from item at index, as obtained from the label string
FXString FileList::getItemFullPathname(int index) const
{
    if ((FXuint)index >= (FXuint)items.no())
    {
        fprintf(stderr, "%s::getItemFullPathname: index out of range.\n", getClassName());
        exit(EXIT_FAILURE);
    }
    FXString label = items[index]->getText();
    return(label.rafter('\t'));
}


// Get associations (if any) from the file
FileAssoc* FileList::getItemAssoc(int index) const
{
    if ((FXuint)index >= (FXuint)items.no())
    {
        fprintf(stderr, "%s::getItemAssoc: index out of range.\n", getClassName());
        exit(EXIT_FAILURE);
    }
    return(((FileItem*)items[index])->assoc);
}


// Return file size of the item
FXulong FileList::getItemFileSize(int index) const
{
    if ((FXuint)index >= (FXuint)items.no())
    {
        fprintf(stderr, "%s::getItemFileSize: index out of range.\n", getClassName());
        exit(EXIT_FAILURE);
    }
    return(((FileItem*)items[index])->size);
}


// Change associations table; force a rescan so as to
// update the bindings in each item to the new associations
void FileList::setAssociations(FileDict* assocs)
{
    if (associations != assocs)
    {
        associations = assocs;
        scan(true);
    }
    setFocus();
}


// Change header size
void FileList::setHeaderSize(int index, int size)
{
    if (index == NB_HEADERS)
    {
        origpathsize = size;
    }
    else if (index == NB_HEADERS+1)
    {
        deldatesize = size;
    }
    else
    {
        if ((FXuint)index >= (FXuint)header->getNumItems())
        {
            fprintf(stderr, "%s::setHeaderSize: index out of range.\n", getClassName());
            exit(EXIT_FAILURE);
        }
        header->setItemSize(index, size);
    }
}


// Get header size
int FileList::getHeaderSize(int index) const
{
    if (index == NB_HEADERS)
    {
        return(origpathsize);
    }

    if (index == NB_HEADERS+1)
    {
        return(deldatesize);
    }

    if ((FXuint)index >= (FXuint)header->getNumItems())
    {
        fprintf(stderr, "%s::getHeaderSize: index out of range.\n", getClassName());
        exit(EXIT_FAILURE);
    }

    return(header->getItemSize(index));
}


// Update refresh timer if the window is unminimized
long FileList::onUpdRefreshTimer(FXObject*, FXSelector, void*)
{
    static FXbool prevMinimized = true;
    static FXbool minimized = true;

    prevMinimized = minimized;
    if (((FXTopWindow*)focuswindow)->isMinimized())
    {
        minimized = false;
    }
    else
    {
        minimized = true;
    }

    // Update refresh timer
    if ((prevMinimized == false) && (minimized == true))
    {
        onCmdRefreshTimer(0, 0, 0);
    }

    return(1);
}


// Scan items to see if listing is necessary
void FileList::scan(FXbool force)
{
    // Start wait cursor if refresh forced (use custom function)
    if (force)
    {
        ::setWaitCursor(getApp(), BEGIN_CURSOR);
    }



    // Special case where the file list is a search list
    if (options&_FILELIST_SEARCH)
    {
        FXbool updated = updateItems(force);
        if (updated)
        {
            sortItems(); // Only sort if search list was updated
        }
    }

    // Normal case
    else
    {
        struct stat info;

        // Stat the current directory
        if (::info(directory, info))
        {
            // New date of directory
            FXTime newdate = (FXTime)FXMAX(info.st_mtime, info.st_ctime);

            // Forced, directory date was changed, or failed to get proper date or counter expired
            if (force || (timestamp != newdate) || (counter == 0))
            {
                // And do the refresh
#if defined(linux)
                refreshMtdevices();
#endif
                listItems(force);
                sortItems();

                // Remember when we did this
                timestamp = newdate;
            }
        }

        // Move to higher directory
        else
        {
            setDirectory(FXPath::upLevel(directory));
        }
    }

    // Stop wait cursor if refresh forced (use custom function)
    if (force)
    {
        ::setWaitCursor(getApp(), END_CURSOR);
    }
}


// Update the list (used in a search list)
FXbool FileList::updateItems(FXbool force)
{
    FXString      grpid, usrid, atts, mod, ext, del;
    FXString      filename, dirname, pathname;
    FileItem*     item;
    FileAssoc*    fileassoc;
    FXString      filetype, lowext;
    FXIcon*       big, *mini;
    FXIcon*       bigthumb = NULL, *minithumb = NULL;
    FXIconSource* source;
    time_t        filemtime, filectime;
    struct stat   info, linfo;
    FXbool        isLink, isBrokenLink;
    FXbool        updated = false;

    // Loop over the item list
    for (int u = 0; u < getNumItems(); u++)
    {
        // Current item
        item = (FileItem*)getItem(u);
        pathname = getItemFullPathname(u);
        dirname = FXPath::directory(pathname);
        filename = FXPath::name(pathname);

        // Get file/link info and indicate if it is a link
        if (lstatrep(pathname.text(), &linfo) != 0)
        {
            removeItem(u, true);
            u--;
            continue;
        }
        isLink = S_ISLNK(linfo.st_mode);
        isBrokenLink = false;

        // Find if it is a broken link
        if (isLink && (statrep(pathname.text(), &info) != 0))
        {
            isBrokenLink = true;
        }

        // File times
        filemtime = linfo.st_mtime;
        filectime = linfo.st_ctime;

        // Update only if the item has changed (mtime or ctime)
        if (force || (item->date != filemtime) || (item->cdate != filectime))
        {
            // Indicate that the list was updated
            updated = true;

            // Obtain user name
            usrid = FXSystem::userName(linfo.st_uid);

            // Obtain group name
            grpid = FXSystem::groupName(linfo.st_gid);

            // Permissions (caution : we don't use the FXSystem::modeString() function because
            // it seems to be incompatible with the info.st_mode format)
            atts = ::permissions(linfo.st_mode);

            // Mod time
            mod = FXSystem::time("%x %X", filemtime);

            del = "";
            ext = "";
            // Obtain the extension for files only
            if (!S_ISDIR(linfo.st_mode))
            {
                ext = FXPath::name(pathname).rafter('.', 2).lower();
                if ((ext == "tar.gz") || (ext == "tar.bz2") || (ext == "tar.xz") || (ext == "tar.z")) // Special cases
                {
					// Do nothing
                }
                else
                {
                    ext = FXPath::extension(pathname).lower();
                }
            }

            // Obtain the stat info on the file itself
            if (statrep(pathname.text(), &info) != 0)
            {
                // Except in the case of a broken link
                if (isBrokenLink)
                {
                    lstatrep(pathname.text(), &info);
                }
                else
                {
                    removeItem(u, true);
                    u--;
                    continue;
                }
            }

            // Set item flags from the obtained info
            if (S_ISDIR(info.st_mode))
            {
                item->state |= FileItem::FOLDER;
            }
            else
            {
                item->state &= ~FileItem::FOLDER;
            }
            if (S_ISLNK(info.st_mode))
            {
                item->state |= FileItem::SYMLINK;
            }
            else
            {
                item->state &= ~FileItem::SYMLINK;
            }
            if (S_ISCHR(info.st_mode))
            {
                item->state |= FileItem::CHARDEV;
            }
            else
            {
                item->state &= ~FileItem::CHARDEV;
            }
            if (S_ISBLK(info.st_mode))
            {
                item->state |= FileItem::BLOCKDEV;
            }
            else
            {
                item->state &= ~FileItem::BLOCKDEV;
            }
            if (S_ISFIFO(info.st_mode))
            {
                item->state |= FileItem::FIFO;
            }
            else
            {
                item->state &= ~FileItem::FIFO;
            }
            if (S_ISSOCK(info.st_mode))
            {
                item->state |= FileItem::SOCK;
            }
            else
            {
                item->state &= ~FileItem::SOCK;
            }
            if ((info.st_mode&(S_IXUSR|S_IXGRP|S_IXOTH)) && !(S_ISDIR(info.st_mode) || S_ISCHR(info.st_mode) || S_ISBLK(info.st_mode) || S_ISFIFO(info.st_mode) || S_ISSOCK(info.st_mode)))
            {
                item->state |= FileItem::EXECUTABLE;
            }
            else
            {
                item->state &= ~FileItem::EXECUTABLE;
            }

            // We can drag items
            item->state |= FileItem::DRAGGABLE;

            // Assume no associations
            fileassoc = NULL;

            // Determine icons and type
            if (item->state&FileItem::FOLDER)
            {
                if (!::isReadExecutable(pathname))
                {
                    big = bigfolderlockedicon;
                    mini = minifolderlockedicon;
                    filetype = _("Folder");
                }
                else
                {
                    big = bigfoldericon;
                    mini = minifoldericon;
                    filetype = _("Folder");
                }
            }
            else if (item->state&FileItem::CHARDEV)
            {
                big = bigchardevicon;
                mini = minichardevicon;
                filetype = _("Character Device");
            }
            else if (item->state&FileItem::BLOCKDEV)
            {
                big = bigblockdevicon;
                mini = miniblockdevicon;
                filetype = _("Block Device");
            }
            else if (item->state&FileItem::FIFO)
            {
                big = bigpipeicon;
                mini = minipipeicon;
                filetype = _("Named Pipe");
            }
            else if (item->state&FileItem::SOCK)
            {
                big = bigsocketicon;
                mini = minisocketicon;
                filetype = _("Socket");
            }
            else if (item->state&FileItem::EXECUTABLE)
            {
                big = bigexecicon;
                mini = miniexecicon;
                filetype = _("Executable");
                if (associations)
                {
                    fileassoc = associations->findFileBinding(pathname.text());
                }
            }
            else
            {
                big = bigdocicon;
                mini = minidocicon;
                filetype = _("Document");
                if (associations)
                {
                    fileassoc = associations->findFileBinding(pathname.text());
                }
            }

            // If association is found, use it
            if (fileassoc)
            {
                filetype = fileassoc->extension.text();

                if (fileassoc->bigicon)
                {
                    big = fileassoc->bigicon;
                }
                if (fileassoc->miniicon)
                {
                    mini = fileassoc->miniicon;
                }
            }

            // Symbolic links have a specific type
            if (isBrokenLink)
            {
                filetype = _("Broken link");
            }

            else if (isLink)
            {
                if (associations)
                {
                    // Don't forget to remove trailing '/' here!
                    fileassoc = associations->findFileBinding(::cleanPath(::readLink(pathname)).text());
                    if (fileassoc)
                    {
                        filetype = _("Link to ")+fileassoc->extension;
                    }
                    else
                    {
                        filetype = _("Link to ")+filetype;
                    }
                }
            }

            // Don't display the file size for directories
            FXString hsize;
            if (S_ISDIR(linfo.st_mode))
            {
                hsize = "";
            }
            else
            {
                char size[64];
#if __WORDSIZE == 64
                snprintf(size, sizeof(size)-1, "%lu", (FXulong)linfo.st_size);
#else
                snprintf(size, sizeof(size)-1, "%llu", (FXulong)linfo.st_size);
#endif
                hsize = ::hSize(size);
            }

            // Set item icons
            item->setBigIcon(big);
            item->setMiniIcon(mini);

            // Attempt to load thumbnails for image files
            if (displaythumbnails)
            {
                // Load big icon from file
                bigthumb = NULL;
                minithumb = NULL;
                if (associations)
                {
                    source = associations->getIconDict()->getIconSource();
                    if (!(item->state&FileItem::FIFO)) // Avoid pipes
                    {
                        bigthumb = source->loadIconFile(pathname);
                    }
                }

                if (bigthumb)
                {
                    register FXuint w = bigthumb->getWidth();
                    register FXuint h = bigthumb->getHeight();

                    // Eventually scale the big icon (best quality)
                    FXuint max_mini_thumb_size = FXIntVal(getApp()->reg().readStringEntry("OPTIONS", "max_mini_thumb_size", "20"));
                    FXuint max_big_thumb_size = FXIntVal(getApp()->reg().readStringEntry("OPTIONS", "max_big_thumb_size", "160"));

                    if ((w > max_big_thumb_size) || (h > max_big_thumb_size))
                    {
                        if (w > h)
                        {
                            bigthumb->scale(max_big_thumb_size, (max_big_thumb_size*h)/w, 1);
                        }
                        else
                        {
                            bigthumb->scale((max_big_thumb_size*w)/h, max_big_thumb_size, 1);
                        }

                        // Size has changed
                        w = bigthumb->getWidth();
                        h = bigthumb->getHeight();
                    }

                    // Copy the big icon to the mini icon (faster than direct rescaling)
                    minithumb = new FXIcon(getApp());
                    FXColor* tmpdata;
                    if (!FXMEMDUP(&tmpdata, bigthumb->getData(), FXColor, w*h))
                    {
                        throw FXMemoryException(_("Unable to load image"));
                    }
                    minithumb->setData(tmpdata, IMAGE_OWNED, w, h);

                    // Eventually scale the mini icon (best quality)
                    w = minithumb->getWidth();
                    h = minithumb->getHeight();
                    if ((w > max_mini_thumb_size) || (h > max_mini_thumb_size))
                    {
                        if (w > h)
                        {
                            minithumb->scale(max_mini_thumb_size, (max_mini_thumb_size*h)/w, 1);
                        }
                        else
                        {
                            minithumb->scale((max_mini_thumb_size*w)/h, max_mini_thumb_size, 1);
                        }
                    }

                    // Set thumbnail icons as owned
                    if (!isLink && !isBrokenLink)
                    {
                        item->setBigIcon(bigthumb, true);
                        item->setMiniIcon(minithumb, true);
                    }
                }
            }

            // Set other item attributes
            item->size = (FXulong)linfo.st_size;
            item->assoc = fileassoc;
            item->date = filemtime;
            item->cdate = filectime;

#if defined(linux)
            // Devices have a specific icon
            if (fsdevices->find(pathname.text()))
            {
                filetype = _("Mount point");

                if (streq(fsdevices->find(pathname.text()), "harddisk"))
                {
                    item->setBigIcon(bigharddiskicon);
                    item->setMiniIcon(harddiskicon);
                }
                else if (streq(fsdevices->find(pathname.text()), "nfsdisk"))
                {
                    item->setBigIcon(bignfsdriveicon);
                    item->setMiniIcon(nfsdriveicon);
                }
                else if (streq(fsdevices->find(pathname.text()), "smbdisk"))
                {
                    item->setBigIcon(bignfsdriveicon);
                    item->setMiniIcon(nfsdriveicon);
                }
                else if (streq(fsdevices->find(pathname.text()), "floppy"))
                {
                    item->setBigIcon(bigfloppyicon);
                    item->setMiniIcon(floppyicon);
                }
                else if (streq(fsdevices->find(pathname.text()), "cdrom"))
                {
                    item->setBigIcon(bigcdromicon);
                    item->setMiniIcon(cdromicon);
                }
                else if (streq(fsdevices->find(pathname.text()), "zip"))
                {
                    item->setBigIcon(bigzipicon);
                    item->setMiniIcon(zipicon);
                }
            }
#endif

            // Update item label
            // NB : Item del is empty if we are not in trash can
            //      Item pathname is not displayed but is used in the tooltip

            if (dirname.length() > 256)
            {
                dirname = dirname.trunc(256) + "[...]";   // Truncate directory path name to 25 characters to prevent overflow
            }
            item->label.format("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s",
                               filename.text(), dirname.text(), hsize.text(), filetype.text(), ext.text(),
                               mod.text(), usrid.text(), grpid.text(), atts.text(),
                               del.text(), pathname.text());

            // Symbolic links have a specific icon
            if (isLink)
            {
                // Broken link
                if (isBrokenLink)
                {
                    item->setBigIcon(bigbrokenlinkicon);
                    item->setMiniIcon(minibrokenlinkicon);
                }
                else
                {
                    item->setBigIcon(biglinkicon);
                    item->setMiniIcon(minilinkicon);
                }
            }
        }

        // Finally don't forget to create the item!
        item->create();

/*
        // Refresh the GUI if an image has to be drawn and recompute the icon height
        if (displaythumbnails)
        {
            if (updated && bigthumb && minithumb && (u < REFRESH_COUNT))
            {
                update();
                recompute();
                makeItemVisible(0); // Fix some refresh problems
                repaint();
            }
        }
*/
    }

    // Gotta recalc size of content
    if (updated)
    {
        recalc();
    }

    return(updated);
}


// List directory (used in a standard file list)
void FileList::listItems(FXbool force)
{
    FileItem       *oldlist, *newlist;
    FileItem       **po, **pn, **pp;
    FXString       grpid, usrid, atts, mod, ext, del, origpath;
    FXString       name, dirname, pathname;
    FileItem       *curitem = NULL;
    FileItem       *item, *link;
    FileAssoc      *fileassoc;
    FXString       filetype, lowext, timeformat;
    FXIconSource   *source;
    FXIcon         *big, *mini;
    FXIcon         *bigthumb = NULL, *minithumb = NULL;
    time_t         filemtime, filectime;
    struct stat    info, linfo;
    struct dirent  *dp;
    DIR*           dirp;
    FXbool         isInTrash, isLink, isBrokenLink, isLinkToDir;
    FXlong         deldate;

    // Read time format
    timeformat = getApp()->reg().readStringEntry("SETTINGS", "time_format", DEFAULT_TIME_FORMAT);

    // Build old and new insert-order lists
    oldlist = list;
    newlist = NULL;

    // Head of old and new lists
    po = &oldlist;
    pn = &newlist;

    // Remember current item
    if (0 <= current)
    {
        curitem = (FileItem*)items[current];
    }

    // Start inserting
    items.clear();

    // Get info about directory
    if (statrep(directory.text(), &info) == 0)
    {
        // Need latest change no matter what actually changed!
        timestamp = FXMAX(info.st_mtime, info.st_ctime);

        // Set path to stat with
        dirname = directory.text();

        if (dirname != ROOTDIR)
        {
            dirname += PATHSEPSTRING;
        }

        // Add the deletion time and original path headers if we are in trash can
        if (dirname.left(trashfileslocation.length()) == trashfileslocation)
        {
            if (getNumHeaders() == NB_HEADERS)
            {
                appendHeader(_("Original path"), NULL, origpathsize);
                appendHeader(_("Deletion date"), NULL, deldatesize);
            }
            isInTrash = true;
        }
        // Eventually remove the deletion and original path headers if we are not in trash can
        else
        {
            if (getNumHeaders() == NB_HEADERS+2)
            {
                deldatesize = header->getItemSize(NB_HEADERS+1);
                removeHeader(NB_HEADERS);
                origpathsize = header->getItemSize(NB_HEADERS);
                removeHeader(NB_HEADERS);

                // Change back the sort function to default if necessary
                if ((sortfunc == ascendingOrigpath) || (sortfunc == ascendingOrigpathMix) ||
                    (sortfunc == descendingOrigpath) || (sortfunc == descendingOrigpathMix))
                {
                    sortfunc = ascendingCase;
                    setSortHeader(0);
                }
                if ((sortfunc == ascendingDeltime) || (sortfunc == ascendingDeltimeMix) ||
                    (sortfunc == descendingDeltime) || (sortfunc == descendingDeltimeMix))
                {
                    sortfunc = ascendingCase;
                    setSortHeader(0);
                }
            }
            isInTrash = false;
        }

        // Get directory stream pointer
        dirp = opendir(directory.text());

        // Managed to open directory
        if (dirp)
        {
            // Loop over directory entries
            while ((dp = readdir(dirp)) != NULL)
            {
                // Directory entry name
                name = dp->d_name;

                // Hidden file (.xxx) or directory (. or .yyy) normally not shown,
                // but directory .. is always shown so we can navigate up or down
                // Hidden files in the trash can base directory are always shown
                if ((name[0] == '.') && ((name[1] == 0) || (!((name[1] == '.') && (name[2] == 0)) && !(options&_FILELIST_SHOWHIDDEN) &&
                                                            (dirname != trashfileslocation+PATHSEPSTRING))))
                {
                    continue;
                }

				if (getApp()->reg().readUnsignedEntry("OPTIONS", "folder_limit", false) == true)
				{
					//printf("folder_limit\n");
					FXString homedir = FXSystem::getHomeDirectory();
					FXString aaa = (directory == "/") ? "" : directory;
					aaa += "/" + name;
					//printf("file %s\n", aaa.text());

					bool bypass = true;
					FXString folder_paths = getApp()->reg().readStringEntry("OPTIONS", "folder_limit_folders", "/mnt:/media");
					folder_paths = (streq(folder_paths.text(), "")) ? "/mnt:/media" : folder_paths;
					FXString this_folder = "";
					for (int x=0; x <= folder_paths.contains(":"); x++)
					{
						this_folder = folder_paths.section(":", x);
						if (streq(aaa.left(this_folder.length()).text(), this_folder.text()))
						{
							bypass = false;
						}
					}

					if (
						bypass == true
						&& ! streq(homedir.left(aaa.length()).text(), aaa.text())
						&& ! streq(aaa.left(homedir.length()).text(), homedir.text())
					)
					{
						continue;
					}
				}

                // Build full pathname
                pathname = dirname+name;

                // Get file/link info and indicate if it is a link
                if (lstatrep(pathname.text(), &linfo) != 0)
                {
                    continue;
                }
                isLink = S_ISLNK(linfo.st_mode);
                isBrokenLink = false;
                isLinkToDir = false;

                // Find if it is a broken link or a link to a directory
                if (isLink)
                {
                    if (statrep(pathname.text(), &info) != 0)
                    {
                        isBrokenLink = true;
                    }
                    else if (S_ISDIR(info.st_mode))
                    {
                        isLinkToDir = true;
                    }
                }

                if (hiddenFolders() && (isLinkToDir || S_ISDIR(linfo.st_mode) || isBrokenLink))
                {
                    continue;
                }

                // If not a directory, nor a link to a directory and we want only directories, skip it
                if (!isLinkToDir && !S_ISDIR(linfo.st_mode) && (options&_FILELIST_SHOWDIRS))
                {
                    continue;
                }

                // Is it a directory or does it match the pattern?
                fileassoc = associations->findFileBinding(pathname.text());
                filetype = (fileassoc) ? fileassoc->extension : "";

                if (!S_ISDIR(linfo.st_mode)
                    && ((!match_by_type && !FXPath::match(pattern, name, matchmode))
                    || (match_by_type && ! FXPath::match(pattern.text(), filetype.text(), FILEMATCH_CASEFOLD)))
                    )
                {
                    continue;
                }

                // Assume no associations
                fileassoc = NULL;

                // File times
                filemtime = linfo.st_mtime;
                filectime = linfo.st_ctime;

                // Find it, and take it out from the old list if found
                for (pp = po; (item = *pp) != NULL; pp = &item->link)
                {
                    if (fileequal(item->label, name))
                    {
                        *pp = item->link;
                        item->link = NULL;
                        po = pp;
                        goto fnd;
                    }
                }

                // Make new item if we have to
                item = (FileItem*)createItem(FXString::null, NULL, NULL, NULL);

                // Append item in list
fnd:
                *pn = item;
                pn = &item->link;

                // Append
                if (item == curitem)
                {
                    current = items.no();
                }
                items.append(item);

                // Update only if forced, or if the item has changed (mtime or ctime)
                if (force || (item->date != filemtime) || (item->cdate != filectime))
                {
                    // Obtain user name
                    usrid = FXSystem::userName(linfo.st_uid);

                    // Obtain group name
                    grpid = FXSystem::groupName(linfo.st_gid);

                    // Permissions (caution : we don't use the FXSystem::modeString() function because
                    // it seems to be incompatible with the info.st_mode format)
                    atts = ::permissions(linfo.st_mode);

                    // Mod time
                    mod = FXSystem::time(timeformat.text(), filemtime);

                    // If we are in trash can, obtain the deletion time and the original path
                    deldate = 0;
                    del = "";
                    ext = "";
                    origpath = "";
                    FXString delstr = "";
                    if (isInTrash)
                    {
                        // Obtain trash base name and sub path
                        FXString subpath = dirname;
                        subpath.erase(0, trashfileslocation.length()+1);
                        FXString trashbasename = subpath.before('/');
                        if (trashbasename == "")
                        {
                            trashbasename = name;
                        }
                        subpath.erase(0, trashbasename.length());

                        // Read the .trashinfo file
                        FILE*    fp;
                        char     line[1024];
                        FXbool   success = true;
                        FXString trashinfopathname = trashinfolocation+PATHSEPSTRING+trashbasename+".trashinfo";
                        if ((fp = fopen(trashinfopathname.text(), "r")) != NULL)
                        {
                            // Read the first three lines and get the strings
                            if (fgets(line, sizeof(line), fp) == NULL)
                            {
                                success = false;
                            }
                            if (fgets(line, sizeof(line), fp) == NULL)
                            {
                                success = false;
                            }
                            if (success)
                            {
                                origpath = line;
                                origpath = origpath.after('=');
                                origpath = origpath.before('\n');
                            }
                            if (fgets(line, sizeof(line), fp) == NULL)
                            {
                                success = false;
                            }
                            if (success)
                            {
                                delstr = line;
                                delstr = delstr.after('=');
                                delstr = delstr.before('\n');
                            }
                            fclose(fp);
                        }

                        // Eventually include sub path in the original path
                        if (subpath == "")
                        {
                            origpath = origpath+subpath;
                        }
                        else
                        {
                            origpath = origpath+subpath+name;
                        }
                        if (delstr == "")
                        {
                            origpath = "";
                        }

                        // Special case
                        if (name == "..")
                        {
                            origpath = "";
                            delstr = "";
                        }

                        // Convert date
                        deldate = deltime(delstr);
                        if (deldate != 0)
                        {
                            del = FXSystem::time(timeformat.text(), deldate);
                        }

                        // Obtain the extension for files only
                        if (!S_ISDIR(linfo.st_mode))
                        {
                            ext = "";
                            if (dirname == trashfileslocation)
                            {
                                ext = FXPath::extension(pathname.rbefore('_'));
                            }
                            if (ext == "")
                            {
                                ext = FXPath::name(pathname).rafter('.', 2).lower();
                                if ((ext == "tar.gz") || (ext == "tar.bz2") || (ext == "tar.xz") || (ext == "tar.z")) // Special cases
                                {
                                }
                                else
                                {
                                    ext = FXPath::extension(pathname).lower();
                                }
                            }
                        }
                    }
                    else
                    {
                        // Obtain the extension for files only
                        if (!S_ISDIR(linfo.st_mode))
                        {
                            ext = FXPath::name(pathname).rafter('.', 2).lower();
                            if ((ext == "tar.gz") || (ext == "tar.bz2") || (ext == "tar.xz") || (ext == "tar.z")) // Special cases
                            {
								// Do nothing
                            }
                            else
                            {
                                ext = FXPath::extension(pathname).lower();
                            }
                        }
                    }

                    // Obtain the stat info on the file or the referred file if it is a link
                    if (statrep(pathname.text(), &info) != 0)
                    {
                        // Except in the case of a broken link
                        if (isBrokenLink)
                        {
                            lstatrep(pathname.text(), &info);
                        }
                        else
                        {
                            continue;
                        }
                    }

                    // Set item flags from the obtained info or linfo
                    if (S_ISDIR(info.st_mode))
                    {
                        item->state |= FileItem::FOLDER;
                    }
                    else
                    {
                        item->state &= ~FileItem::FOLDER;
                    }
                    if (S_ISLNK(linfo.st_mode))
                    {
                        item->state |= FileItem::SYMLINK;
                    }
                    else
                    {
                        item->state &= ~FileItem::SYMLINK;
                    }
                    if (S_ISCHR(info.st_mode))
                    {
                        item->state |= FileItem::CHARDEV;
                    }
                    else
                    {
                        item->state &= ~FileItem::CHARDEV;
                    }
                    if (S_ISBLK(info.st_mode))
                    {
                        item->state |= FileItem::BLOCKDEV;
                    }
                    else
                    {
                        item->state &= ~FileItem::BLOCKDEV;
                    }
                    if (S_ISFIFO(info.st_mode))
                    {
                        item->state |= FileItem::FIFO;
                    }
                    else
                    {
                        item->state &= ~FileItem::FIFO;
                    }
                    if (S_ISSOCK(info.st_mode))
                    {
                        item->state |= FileItem::SOCK;
                    }
                    else
                    {
                        item->state &= ~FileItem::SOCK;
                    }
                    if ((info.st_mode&(S_IXUSR|S_IXGRP|S_IXOTH)) && !(S_ISDIR(info.st_mode) || S_ISCHR(info.st_mode) || S_ISBLK(info.st_mode) || S_ISFIFO(info.st_mode) || S_ISSOCK(info.st_mode)))
                    {
                        item->state |= FileItem::EXECUTABLE;
                    }
                    else
                    {
                        item->state &= ~FileItem::EXECUTABLE;
                    }

                    filetype = "";
                    // We can drag items
                    item->state |= FileItem::DRAGGABLE;

                    // Determine icons and type
                    if (item->state&FileItem::FOLDER)
                    {
                        if (!::isReadExecutable(pathname))
                        {
                            big = bigfolderlockedicon;
                            mini = minifolderlockedicon;
                            filetype = _("Folder");
                        }
                        else
                        {
                            big = bigfoldericon;
                            mini = minifoldericon;
                            filetype = _("Folder");
                        }
                    }
                    else if (item->state&FileItem::CHARDEV)
                    {
                        big = bigchardevicon;
                        mini = minichardevicon;
                        filetype = _("Character Device");
                    }
                    else if (item->state&FileItem::BLOCKDEV)
                    {
                        big = bigblockdevicon;
                        mini = miniblockdevicon;
                        filetype = _("Block Device");
                    }
                    else if (item->state&FileItem::FIFO)
                    {
                        big = bigpipeicon;
                        mini = minipipeicon;
                        filetype = _("Named Pipe");
                    }
                    else if (item->state&FileItem::SOCK)
                    {
                        big = bigsocketicon;
                        mini = minisocketicon;
                        filetype = _("Socket");
                    }
                    else if (item->state&FileItem::EXECUTABLE)
                    {
                        big = bigexecicon;
                        mini = miniexecicon;
                        filetype = _("Executable");
                        if (associations)
                        {
                            fileassoc = associations->findFileBinding(pathname.text());
                        }
                    }
                    else
                    {
                        big = bigdocicon;
                        mini = minidocicon;
                        filetype = _("Document");
                        if (associations)
                        {
                            // Eventually strip the '_' suffix when we are in trash root
                            if (dirname == trashfileslocation)
                            {
                                FXString stripname = pathname.rbefore('_');
                                if (stripname == "")
                                {
                                    fileassoc = associations->findFileBinding(pathname.text());
                                }
                                else
                                {
                                    fileassoc = associations->findFileBinding(stripname.text());
                                }
                            }
                            else
                            {
                                fileassoc = associations->findFileBinding(pathname.text());
                            }
                        }
                    }

                    // If association is found, use it
                    if (fileassoc)
                    {
                        if ((fileassoc->extension != "") || !(item->state&FileItem::EXECUTABLE))
                        {
                            filetype = fileassoc->extension.text();
                        }
                        if (fileassoc->bigicon)
                        {
                            big = fileassoc->bigicon;
                        }
                        if (fileassoc->miniicon)
                        {
                            mini = fileassoc->miniicon;
                        }
                    }

                    // Symbolic links have a specific type
                    if (isBrokenLink)
                    {
                        filetype = _("Broken link");
                    }

                    else if (isLink)
                    {
                        if (associations)
                        {
                            // Don't forget to remove trailing '/' here!
                            fileassoc = associations->findFileBinding(::cleanPath(::readLink(pathname)).text());
                            if (fileassoc && (fileassoc->extension != ""))
                            {
                                filetype = _("Link to ")+fileassoc->extension;
                            }

                            // If no association found, get the link file type from the referred file type
                            else
                            {
                                if (item->state&FileItem::FOLDER)
                                {
                                    filetype = _("Folder");
                                }
                                else if (item->state&FileItem::CHARDEV)
                                {
                                    filetype = _("Character Device");
                                }
                                else if (item->state&FileItem::BLOCKDEV)
                                {
                                    filetype = _("Block Device");
                                }
                                else if (item->state&FileItem::FIFO)
                                {
                                    filetype = _("Named Pipe");
                                }
                                else if (item->state&FileItem::SOCK)
                                {
                                    filetype = _("Socket");
                                }
                                else if (item->state&FileItem::EXECUTABLE)
                                {
                                    filetype = _("Executable");
                                }
                                else
                                {
                                    filetype = _("Document");
                                }

                                filetype = _("Link to ")+filetype;
                            }
                        }
                    }

                    // Don't display the file size for directories
                    FXString hsize;
                    if (S_ISDIR(linfo.st_mode))
                    {
                        hsize = "";
                    }
                    else
                    {
                        char size[64];
#if __WORDSIZE == 64
                        snprintf(size, sizeof(size)-1, "%lu", (FXulong)linfo.st_size);
#else
                        snprintf(size, sizeof(size)-1, "%llu", (FXulong)linfo.st_size);
#endif
                        hsize = ::hSize(size);
                    }

                    // Set item icons
                    item->setBigIcon(big);
                    item->setMiniIcon(mini);

                    // Attempt to load thumbnails for image files
                    if (displaythumbnails)
                    {
						 FXuint max_mini_thumb_size = FXIntVal(getApp()->reg().readStringEntry("OPTIONS", "max_mini_thumb_size", "20"));
						 FXuint max_big_thumb_size = FXIntVal(getApp()->reg().readStringEntry("OPTIONS", "max_big_thumb_size", "160"));
						 // printf("thumb %i, %i\n", max_mini_thumb_size, max_big_thumb_size);

                        // Load big icon from file
                        bigthumb = NULL;
                        minithumb = NULL;
                        if (associations)
                        {
                            source = associations->getIconDict()->getIconSource();
                            if (!(item->state&FileItem::FIFO)) // Avoid pipes
                            {
                                bigthumb = source->loadIconFile(pathname);
                            }
                        }

                        if (bigthumb)
                        {
                            register FXuint w = bigthumb->getWidth();
                            register FXuint h = bigthumb->getHeight();

                            // Eventually scale the big icon (best quality)
                            if ((w > max_big_thumb_size) || (h > max_big_thumb_size))
                            {
                                if (w > h)
                                {
                                    bigthumb->scale(max_big_thumb_size, (max_big_thumb_size*h)/w, 1);
                                }
                                else
                                {
                                    bigthumb->scale((max_big_thumb_size*w)/h, max_big_thumb_size, 1);
                                }

                                // Size has changed
                                w = bigthumb->getWidth();
                                h = bigthumb->getHeight();
                            }

                            // Copy the big icon to the mini icon (faster than direct rescaling)
                            minithumb = new FXIcon(getApp());
                            FXColor* tmpdata;
                            if (!FXMEMDUP(&tmpdata, bigthumb->getData(), FXColor, w*h))
                            {
                                throw FXMemoryException(_("Unable to load image"));
                            }
                            minithumb->setData(tmpdata, IMAGE_OWNED, w, h);

                            // Eventually scale the mini icon (best quality)
                            w = minithumb->getWidth();
                            h = minithumb->getHeight();
                            if ((w > max_mini_thumb_size) || (h > max_mini_thumb_size))
                            {
                                if (w > h)
                                {
                                    minithumb->scale(max_mini_thumb_size, (max_mini_thumb_size*h)/w, 1);
                                }
                                else
                                {
                                    minithumb->scale((max_mini_thumb_size*w)/h, max_mini_thumb_size, 1);
                                }
                            }

                            // Set thumbnail icons as owned
                            if (!isLink && !isBrokenLink)
                            {
                                item->setBigIcon(bigthumb, true);
                                item->setMiniIcon(minithumb, true);
                            }
                        }
                    }

                    // Set other item attributes
                    item->size = (FXulong)linfo.st_size;
                    item->assoc = fileassoc;
                    item->date = filemtime;
                    item->cdate = filectime;
                    item->deldate = deldate;

#if defined(linux)
                    // Mounted devices may have a specific icon
                    if (mtdevices->find(pathname.text()))
                    {
                        filetype = _("Mount point");

                        if (streq(mtdevices->find(pathname.text()), "cifs"))
                        {
                            item->setBigIcon(bignfsdriveicon);
                            item->setMiniIcon(nfsdriveicon);
                        }
                        else
                        {
                            item->setBigIcon(bigharddiskicon);
                            item->setMiniIcon(harddiskicon);
                        }
                    }

                    // Devices found in fstab may have a specific icon
                    if (fsdevices->find(pathname.text()))
                    {
                        filetype = _("Mount point");

                        if (streq(fsdevices->find(pathname.text()), "harddisk"))
                        {
                            item->setBigIcon(bigharddiskicon);
                            item->setMiniIcon(harddiskicon);
                        }
                        else if (streq(fsdevices->find(pathname.text()), "nfsdisk"))
                        {
                            item->setBigIcon(bignfsdriveicon);
                            item->setMiniIcon(nfsdriveicon);
                        }
                        else if (streq(fsdevices->find(pathname.text()), "smbdisk"))
                        {
                            item->setBigIcon(bignfsdriveicon);
                            item->setMiniIcon(nfsdriveicon);
                        }
                        else if (streq(fsdevices->find(pathname.text()), "floppy"))
                        {
                            item->setBigIcon(bigfloppyicon);
                            item->setMiniIcon(floppyicon);
                        }
                        else if (streq(fsdevices->find(pathname.text()), "cdrom"))
                        {
                            item->setBigIcon(bigcdromicon);
                            item->setMiniIcon(cdromicon);
                        }
                        else if (streq(fsdevices->find(pathname.text()), "zip"))
                        {
                            item->setBigIcon(bigzipicon);
                            item->setMiniIcon(zipicon);
                        }
                    }
#endif

                    // Update item label
                    // NB : Item del is empty if we are not in trash can
                    //      Item pathname is not displayed but is used in the tooltip
                    item->label.format("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s",
                                       name.text(), hsize.text(), filetype.text(), ext.text(),
                                       mod.text(), usrid.text(), grpid.text(), atts.text(),
                                       origpath.text(), del.text(), pathname.text());

                    // Dotdot folders have a specific icon
                    if ((name[0] == '.') && (name[1] == '.') && (name[2] == 0))
                    {
                        item->setBigIcon(bigfolderupicon);
                        item->setMiniIcon(minifolderupicon);
                    }

                    // Symbolic links have a specific icon
                    if (isLink)
                    {
                        // Broken link
                        if (isBrokenLink)
                        {
                            item->setBigIcon(bigbrokenlinkicon);
                            item->setMiniIcon(minibrokenlinkicon);
                        }
                        else
                        {
                            item->setBigIcon(biglinkicon);
                            item->setMiniIcon(minilinkicon);
                        }
                    }
                }

                // Create item
                if (id())
                {
                    item->create();
                }

                // Refresh the GUI if an image has to be drawn and recompute the icon height
                // Don't redraw if there are too many images
                if (displaythumbnails)
                {
                    if (bigthumb && minithumb && (items.no() < REFRESH_COUNT))
                    {
                        update();
                        recompute();
                        makeItemVisible(0); // Fix some refresh problems
                        repaint();
                    }
                }
            }
            closedir(dirp);
        }
    }

    // Wipe items remaining in list:- they have disappeared!!
    for (item = oldlist; item; item = link)
    {
        link = item->link;
        delete item;
    }

    // Validate
    if (current >= items.no())
    {
        current = -1;
    }
    if (anchor >= items.no())
    {
        anchor = -1;
    }
    if (extent >= items.no())
    {
        extent = -1;
    }

    // Remember new list
    list = newlist;

    // Gotta recalc size of content
    recalc();
}
