#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QCloseEvent>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/bimap.hpp>
#include <boost/asio.hpp>
#include <string>
#include <vector>

// DirectInput
#include "CommonStates.h"
#include "DirectXHelpers.h"
#include "GamePad.h"

// LSL API
#include <lsl_cpp.h>


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent, const std::string &config_file);
	void detectButtonPress(int index, bool isPressed, double now, bool wasPressed[], lsl::stream_outlet* outletButtons);
	~MainWindow();
    
private slots:
    // config file dialog ops (from main menu)
    void load_config_dialog();
	void refresh_pads();
	void save_config_dialog();

    // start the GameController connection
    void link();

    // close event (potentially disabled)
    void closeEvent(QCloseEvent *ev);
private:
	static BOOL CALLBACK controller_enum_callback(int pdidInstance, VOID *pWindow);
	static BOOL CALLBACK object_enum_callback(const int pdidoi, VOID *pContext);

	BOOL CALLBACK on_controller(const int pdidInstance);

    // background data reader thread
	void read_thread(std::string name);

    // raw config file IO
    void load_config(const std::string &filename);
    void save_config(const std::string &filename);

	bool stop_;											// whether we're trying to stop the reader thread
    boost::shared_ptr<boost::thread> reader_thread_;	// our reader thread

	boost::bimap<int,std::wstring> indexToInstance_;	// map between index in device selection list and instance GUID string

    std::unique_ptr<DirectX::GamePad> m_gamePad;

    Ui::MainWindow *ui;

	typedef struct DIJOYSTATE2 {
		LONG lX;
		LONG lY;
		LONG lZ;
		LONG lRx;
		LONG lRy;
		LONG lRz;
		LONG rglSlider[2];
		DWORD rgdwPOV[4];
		BYTE rgbButtons[128];
		LONG lVX;
		LONG lVY;
		LONG lVZ;
		LONG lVRx;
		LONG lVRy;
		LONG lVRz;
		LONG rglVSlider[2];
		LONG lAX;
		LONG lAY;
		LONG lAZ;
		LONG lARx;
		LONG lARy;
		LONG lARz;
		LONG rglASlider[2];
		LONG lFX;
		LONG lFY;
		LONG lFZ;
		LONG lFRx;
		LONG lFRy;
		LONG lFRz;
		LONG rglFSlider[2];
	} DIJOYSTATE2, * LPDIJOYSTATE2;

};

#endif // MAINWINDOW_H
