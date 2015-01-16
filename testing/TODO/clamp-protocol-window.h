#include "clamp-protocol.h"
#include <QtGui>
#include <settings.h>
#include <vector>
#include <basicplot.h>
#include <boost/shared_ptr.hpp>
#include <qwt_plot_curve.h>

#include <iostream>

typedef boost::shared_ptr<QwtPlotCurve> QwtPlotCurvePtr;    

struct curve_token_t { // Token used in fifo, holds size of curve
	int trial;
	int sweep;
	bool lastStep;
	double period; // Time period while taking data
	size_t points;
	int stepStart; // Actual time sweep started divided by period, used in normal plotting
	int stepStartSweep; // Time used to overlay sweeps, unitless
	double prevSegmentEnd; // Time when previous segment ended if protocol had sweeps = 1 for all segments
};
 
class ClampProtocolWindow : public ClampProtocolWindowUI, public virtual Settings::Object {
	Q_OBJECT

	public:
		ClampProtocolWindow( QWidget *, Panel * );
		virtual ~ClampProtocolWindow( void );

	public slots:
		void addCurve(double *, curve_token_t );
		void doDeferred(const Settings::Object::State &);
		void doLoad(const Settings::Object::State &);
		void doSave(Settings::Object::State &) const;                                                     

	private slots:
		void setAxes( void );
		void clearPlot( void );
		void toggleOverlay( void );
		void togglePlotAfter( void );
		void changeColorScheme( int );

	private:
		void colorCurve( QwtPlotCurvePtr, int );

		BasicPlot *plot;
		Panel *panel;
		std::vector<QwtPlotCurvePtr> curveContainer; // Used to hold curves to control memory allocation and deallocation
		bool overlaySweeps; // True: sweeps are plotted on same time scale
		bool plotAfter; // True: only replot after a protocol has ended, False: replot after each step
		int colorScheme; // 0: color by run, 1: color by trial, 2: color by sweep
		int runCounter; // Used in run color scheme
		int sweepsShown; // Used to keep track of sweeps shown in legend
		QFont font;
	}; // class ClampProtocolWindow    
}; // namespace ClampProtocol

class ClampProtocolWindowUI : public QWidget {
	Q_OBJECT

	public:
		ClampProtocolWindowUI( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
		~ClampProtocolWindowUI();

		QFrame* frame;
		QLabel* currentScaleLabel;
		QComboBox* currentScaleEdit;
		QSpinBox* currentY2Edit;
		QComboBox* timeScaleEdit;
		QSpinBox* timeX2Edit;
		QSpinBox* currentY1Edit;
		QLabel* timeScaleLabel;
		QSpinBox* timeX1Edit;
		QPushButton* setAxesButton;
		QCheckBox* overlaySweepsCheckBox;
		QCheckBox* plotAfterCheckBox;
		QLabel* textLabel1;
		QComboBox* colorByComboBox;
		QPushButton* clearButton;

	protected:
		QHBoxLayout* frameLayout;
		QSpacerItem* spacer;
		QGridLayout* layout1;
		QVBoxLayout* layout2;
		QVBoxLayout* layout3;

	private:
		QPixmap image0;
		QPixmap image1;
};
