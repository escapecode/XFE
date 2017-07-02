#ifndef DIALOGBOX_H
#define DIALOGBOX_H


// Dialog Box window
class FXAPI DialogBox : public FXTopWindow
{
    FXDECLARE(DialogBox)
protected:
    DialogBox() : _option(0)
    {}
    DialogBox(const DialogBox&)
    {}
public:
    long onKeyPress(FXObject*, FXSelector, void*);
    long onKeyRelease(FXObject*, FXSelector, void*);
    long onClose(FXObject*, FXSelector, void*);
    long onCmdAccept(FXObject*, FXSelector, void*);
    long onCmdCancel(FXObject*, FXSelector, void*);
    long onCmdToggleOption(FXObject*, FXSelector, void*);
public:
    enum
    {
        ID_CANCEL=FXTopWindow::ID_LAST,
        ID_ACCEPT,
        ID_TOGGLE_OPTION,
        ID_LAST
    };
public:
    DialogBox(FXWindow* win, const FXString& name, FXuint opts = DECOR_TITLE|DECOR_BORDER, int x = 0, int y = 0, int w = 0, int h = 0, int pl = 10, int pr = 10, int pt = 10, int pb = 10, int hs = 4, int vs = 4);
    DialogBox(FXApp* a, const FXString& name, FXuint opts = DECOR_TITLE|DECOR_BORDER, int x = 0, int y = 0, int w = 0, int h = 0, int pl = 10, int pr = 10, int pt = 10, int pb = 10, int hs = 4, int vs = 4);
    virtual void show(FXuint placement = PLACEMENT_CURSOR);
    virtual void create();
    FXuint execute(FXuint placement = PLACEMENT_CURSOR);
    FXuint getOption();

protected:
    FXuint _option;
};


#endif
