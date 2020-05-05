#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace DirectX;

#define SAFE_DELETE(p)  { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

MainWindow::MainWindow(QWidget *parent, const std::string &config_file) :
QMainWindow(parent), ui(new Ui::MainWindow)
{
	try {
		ui->setupUi(this);

		HRESULT hr;
		// Init DirectInput
		m_gamePad = std::make_unique<GamePad>();

		// parse startup config file
		load_config(config_file);

		// make GUI connections
		QObject::connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(close()));
		QObject::connect(ui->linkButton, SIGNAL(clicked()), this, SLOT(link()));
		QObject::connect(ui->actionLoad_Configuration, SIGNAL(triggered()), this, SLOT(load_config_dialog()));
		QObject::connect(ui->actionSave_Configuration, SIGNAL(triggered()), this, SLOT(save_config_dialog()));
	} catch(std::exception &e) {
		QMessageBox::critical(this,"Error",e.what(),QMessageBox::Ok);
		throw;
	}
}

// enumerates controller features and sets ranges for the axes
/*BOOL CALLBACK MainWindow::object_enum_callback(int pdidoi, VOID *pWindow) {
    if(pdidoi & DIDFT_AXIS) {
        DIPROPRANGE diprg; 
        diprg.diph.dwSize       = sizeof(DIPROPRANGE); 
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
        diprg.diph.dwHow        = DIPH_BYID; 
        diprg.diph.dwObj        = pdidoi->dwType;
        diprg.lMin              = -1000; 
        diprg.lMax              = +1000; 
        // Set the range for the axis
        ((MainWindow*)pWindow)->pController->SetProperty(DIPROP_RANGE,&diprg.diph);
    }
    return DIENUM_CONTINUE;
}*/

// enumerates controllers and populates GUI combobox with them
/*BOOL CALLBACK MainWindow::controller_enum_callback(int pdidInstance, VOID *pWindow) {
	return ((MainWindow*)pWindow)->on_controller(pdidInstance);
}*/

/*BOOL CALLBACK MainWindow::on_controller(int pdidInstance) {
	wchar_t *bstrGuid = NULL;
    StringFromCLSID(pdidInstance,&bstrGuid);
	indexToInstance_.insert(boost::bimap<int,std::wstring>::value_type(ui->deviceSelector->count(),std::wstring(bstrGuid)));
	char instance_name[4096]; wcstombs(instance_name,pdidInstance->tszInstanceName,sizeof(instance_name));
	ui->deviceSelector->addItem(instance_name);
    ::CoTaskMemFree(bstrGuid);
	return DIENUM_CONTINUE;
}*/


void MainWindow::load_config_dialog() {
	QString sel = QFileDialog::getOpenFileName(this,"Load Configuration File","","Configuration Files (*.cfg)");
	if (!sel.isEmpty())
		load_config(sel.toStdString());
}

void MainWindow::refresh_pads() {
	// Enumerate game controllers and add them to the UI
	ui->deviceSelector->clear();

	for (int i = 0; i < 16; i++) {
		auto state = m_gamePad->GetState(i);

		if (state.IsConnected())
		{
			ui->deviceSelector->addItem("Xbox Controller");
		}
	}
}

void MainWindow::save_config_dialog() {
	QString sel = QFileDialog::getSaveFileName(this,"Save Configuration File","","Configuration Files (*.cfg)");
	if (!sel.isEmpty())
		save_config(sel.toStdString());
}

void MainWindow::closeEvent(QCloseEvent *ev) {
	if (reader_thread_)
		ev->ignore();
}

void MainWindow::load_config(const std::string &filename) {
	using boost::property_tree::wptree;
	wptree pt;

	// parse file
	try {
		read_xml(filename, pt);
	} catch(std::exception &e) {
		QMessageBox::information(this,"Error",(std::string("Cannot read config file: ")+= e.what()).c_str(),QMessageBox::Ok);
		return;
	}

	// get config values
	try {
		// pre-select the device in the UI
		std::wstring deviceguid = pt.get<std::wstring>(L"settings.deviceguid",L"");
		if (!deviceguid.empty()) {
			if (!indexToInstance_.right.count(deviceguid)) {
				QMessageBox::information(this,"Error","The previously configured device was not found. Is it plugged in?",QMessageBox::Ok);
			} else
				ui->deviceSelector->setCurrentIndex(indexToInstance_.right.at(deviceguid));
		}
	} catch(std::exception &) {
		QMessageBox::information(this,"Error in Config File","Could not read out config parameters.",QMessageBox::Ok);
		return;
	}
}

void MainWindow::save_config(const std::string &filename) {
	using boost::property_tree::wptree;
	wptree pt;

	// transfer UI content into property tree
	try {
		pt.put(L"settings.deviceguid",indexToInstance_.left.at(ui->deviceSelector->currentIndex()));
	} catch(std::exception &e) {
		QMessageBox::critical(this,"Error",(std::string("Could not prepare settings for saving: ")+=e.what()).c_str(),QMessageBox::Ok);
	}

	// write to disk
	try {
		write_xml(filename, pt);
	} catch(std::exception &e) {
		QMessageBox::critical(this,"Error",(std::string("Could not write to config file: ")+=e.what()).c_str(),QMessageBox::Ok);
	}
}


// start/stop the GameController connection
void MainWindow::link() {
	HRESULT hr;
	if (reader_thread_) {
		// === perform unlink action ===
		try {
			stop_ = true;
			reader_thread_->interrupt();
			reader_thread_->join();
			reader_thread_.reset();
			// unacquire, release and delete everything...
			m_gamePad->Suspend();
		} catch(std::exception &e) {
			QMessageBox::critical(this,"Error",(std::string("Could not stop the background processing: ")+=e.what()).c_str(),QMessageBox::Ok);
			return;
		}

		// indicate that we are now successfully unlinked
		ui->linkButton->setText("Link");
	} else {
		// === perform link action ===
		try {

			refresh_pads();

			auto state = m_gamePad->GetState(ui->deviceSelector->currentIndex());

			if (state.IsConnected())
			{
				// start reading
				stop_ = false;
				reader_thread_.reset(new boost::thread(&MainWindow::read_thread, this, "xbox_controller_"));
			}

			
		}
		catch(std::exception &e) {
			QMessageBox::critical(this,"Error",(std::string("Could not initialize the GameController interface: ")+=e.what()).c_str(),QMessageBox::Ok);
			return;
		}

		// done, all successful
		ui->linkButton->setText("Unlink");
	}
}


// background data reader thread
void MainWindow::read_thread(std::string name) {
	HRESULT hr;

	// create streaminfo and outlet for the button events
	lsl::stream_info infoButtons(name + "Buttons","Markers",1,lsl::IRREGULAR_RATE,lsl::cf_float32,name + "Buttons_" + boost::asio::ip::host_name());
	lsl::stream_outlet outletButtons(infoButtons);

	// create streaminfo and outlet for the axes
	lsl::stream_info infoAxes(name + "Axes","Position",6,60,lsl::cf_float32,name + "_Axes_" + boost::asio::ip::host_name());
	// append some meta-data...
	lsl::xml_element channels = infoAxes.desc().append_child("channels");
	channels.append_child("channel")
		.append_child_value("label","L_X")
		.append_child_value("type","PositionX_L")
		.append_child_value("unit","normalized_signed");
	channels.append_child("channel")
		.append_child_value("label","LY")
		.append_child_value("type","PositionY_L")
		.append_child_value("unit","normalized_signed");
	channels.append_child("channel")
		.append_child_value("label","R_X")
		.append_child_value("type","PositionX_R")
		.append_child_value("unit","normalized_signed");
	channels.append_child("channel")
		.append_child_value("label","R_Y")
		.append_child_value("type","PositionY_R")
		.append_child_value("unit","normalized_signed");
	channels.append_child("channel")
		.append_child_value("label","Trigger_L")
		.append_child_value("type","Rotation_X")
		.append_child_value("unit","normalized_signed");
	channels.append_child("channel")
		.append_child_value("label","Trigger_R")
		.append_child_value("type","Rotation_Y")
		.append_child_value("unit","normalized_signed");
	infoAxes.desc().append_child("acquisition")
		.append_child_value("model",name.c_str());
	lsl::stream_outlet outletAxes(infoAxes);

	// enter transmission loop
	bool waspressed[128] = {false};

	
	DIJOYSTATE2 js;	
	boost::posix_time::ptime t_start = boost::posix_time::microsec_clock::local_time();
	boost::int64_t t=0;
	while (!stop_) {
		// poll the device
		auto state = m_gamePad->GetState(0);

		double now = lsl::local_clock();

		if (state.IsConnected()) {

			// construct the axes sample and send it off
			float sample[6] = {state.thumbSticks.leftX, state.thumbSticks.leftY, state.thumbSticks.rightX, state.thumbSticks.rightY, state.triggers.left, state.triggers.right};

			outletAxes.push_sample(sample, now);

			// generate the button-event samples...
			if (!waspressed[0] && state.IsAPressed()) {
				waspressed[0] = true;
				float buttonValue = 0;
				outletButtons.push_sample(&buttonValue, now);
			} else if (waspressed[0] && !state.IsAPressed()) {
				waspressed[0] = false;
			}

			if (!waspressed[1] && state.IsBPressed()) {

				waspressed[1] = true;
				float buttonValue = 1;
				outletButtons.push_sample(&buttonValue, now);
			} else if (waspressed[1] && !state.IsBPressed()) {
				waspressed[1] = false;
			}


			if (!waspressed[2] && state.IsXPressed()) {

				waspressed[2] = true;
				float buttonValue = 2;
				outletButtons.push_sample(&buttonValue, now);
			} else if (waspressed[2] && !state.IsXPressed()) {
				waspressed[2] = false;
			}


			if (!waspressed[3] && state.IsYPressed()) {

				waspressed[3] = true;
				float buttonValue = 3;
				outletButtons.push_sample(&buttonValue, now);
			} else if (waspressed[3] && !state.IsYPressed()) {
				waspressed[3] = false;
			}

			if (!waspressed[4] && state.IsDPadDownPressed()) {

				waspressed[4] = true;
				float buttonValue = 4;
				outletButtons.push_sample(&buttonValue, now);
			} else if (waspressed[4] && !state.IsDPadDownPressed()) {
				waspressed[4] = false;
			}

			if (!waspressed[5] && state.IsDPadLeftPressed()) {

				waspressed[5] = true;
				float buttonValue = 5;
				outletButtons.push_sample(&buttonValue, now);
			} else if (waspressed[5] && !state.IsDPadLeftPressed()) {
				waspressed[5] = false;
			}

			if (!waspressed[6] && state.IsDPadRightPressed()) {

				waspressed[6] = true;
				std::string text("Dpad Right pressed");
				float buttonValue = 6;
			} else if (waspressed[6] && !state.IsDPadRightPressed()) {
				waspressed[6] = false;
			}

			if (!waspressed[7] && state.IsDPadUpPressed()) {

				waspressed[7] = true;
				std::string text("Dpad Up pressed");
				float buttonValue = 7;
			} else if (waspressed[7] && !state.IsDPadUpPressed()) {
				waspressed[7] = false;
			}

			if (!waspressed[8] && state.IsLeftShoulderPressed()) {

				waspressed[8] = true;
				std::string text("Left Shoulder pressed");
				float buttonValue = 8;
			} else if (waspressed[8] && !state.IsLeftShoulderPressed()) {
				waspressed[8] = false;
			}

			if (!waspressed[9] && state.IsRightShoulderPressed()) {

				waspressed[9] = true;
				std::string text("Right Shoulder pressed");
				float buttonValue = 9;
			} else if (waspressed[9] && !state.IsRightShoulderPressed()) {
				waspressed[9] = false;
			}

			if (!waspressed[10] && state.IsStartPressed()) {

				waspressed[10] = true;
				std::string text("Button Start pressed");
				float buttonValue = 10;
			} else if (waspressed[10] && !state.IsStartPressed()) {
				waspressed[10] = false;
			}


			if (!waspressed[11] && state.IsLeftStickPressed()) {

				waspressed[11] = true;
				std::string text("Left Stick pressed");
				float buttonValue = 11;
			} else if (waspressed[11] && !state.IsLeftStickPressed()) {
				waspressed[11] = false;
			}



			if (!waspressed[12] && state.IsRightStickPressed()) {

				waspressed[12] = true;
				std::string text("Right Stick pressed");
				float buttonValue = 12;
			} else if (waspressed[12] && !state.IsRightStickPressed()) {
				waspressed[12] = false;
			}
		}

			
		boost::this_thread::sleep_for(boost::chrono::milliseconds(16));
	}
}

MainWindow::~MainWindow() {
	delete ui;
}

