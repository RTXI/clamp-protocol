/*
 * Copyright (C) 2011 Weill Medical College of Cornell University
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <QDomDocument>
#include <QWidget>

#include <qwt_plot_curve.h>
#include <rtxi/plot/basicplot.h>
#include <rtxi/widgets.hpp>

class QHBoxLayout;
class QVBoxLayout;
class QSpacerItem;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QListWidgetItem;
class QListWidget;
class QTableWidget;
class QTableWidgetItem;

// This is an generated header file. You may change the namespace, but
// make sure to do the same in implementation (.cpp) file
namespace clamp_protocol
{

constexpr std::string_view MODULE_NAME = "clamp-protocol";

enum PARAMETER : Widgets::Variable::Id
{
  // set parameter ids here
  INTERVAL_TIME = 0,
  NUM_OF_TRIALS,
  LIQUID_JUNCT_POTENTIAL,
  VOLTAGE_FACTOR,
  TRIAL,
  SEGMENT,
  SWEEP,
  TIME
};

inline std::vector<Widgets::Variable::Info> get_default_vars()
{
  return {{INTERVAL_TIME,
           "Interval Time",
           "Time allocated between intervals",
           Widgets::Variable::DOUBLE_PARAMETER,
           0.0},
          {NUM_OF_TRIALS,
           "Number of Trials",
           "Number of times to apply the loaded protocol",
           Widgets::Variable::INT_PARAMETER,
           int64_t {0}},
          {LIQUID_JUNCT_POTENTIAL,
           "Liquid Junct. Potential (mV)",
           "(mV)",
           Widgets::Variable::DOUBLE_PARAMETER,
           0.0},
          {VOLTAGE_FACTOR,
           "Voltage Factor",
           "Scaling factor for output voltage",
           Widgets::Variable::UINT_PARAMETER,
           uint64_t {0}},
          {TRIAL,
           "Trial",
           "Number of the trial currently being run",
           Widgets::Variable::STATE,
           uint64_t {0}},
          {SEGMENT,
           "Segment",
           "Number of the protocol segment being executed",
           Widgets::Variable::STATE,
           uint64_t {0}},
          {SWEEP,
           "Sweep",
           "Sweep number in current segment",
           Widgets::Variable::STATE,
           uint64_t {0}},
          {TIME,
           "Time (ms)",
           "Elapsed time for current trial",
           Widgets::Variable::STATE,
           uint64_t {0}}};
}

inline std::vector<IO::channel_t> get_default_channels()
{
  return {{
              "Current In (A)",
              "Applied current (A)",
              IO::INPUT,
          },
          {
              "Voltage Out (V w/ LJP)",
              "Voltage output with liquid junction potential",
              IO::OUTPUT,
          }};
}

struct data_token_t
{
  int64_t stepStart;
  int64_t time;
  double value;
  int trial;
  int segment;
  int sweep;
  int step;
};

enum ampMode_t : int
{
  VOLTAGE = 0,
  CURRENT
};

enum stepType_t : int
{
  STEP = 0,
  RAMP,
};

// DO NOT REORDER! IF ADDING MORE PARAMETERS INSERT RIGHT BEFORE
// PROTOCOL_PARAMETERS_SIZE!
enum protocol_parameters : size_t
{
  STEP_DURATION = 0,
  DELTA_STEP_DURATION,
  HOLDING_LEVEL_1,
  DELTA_HOLDING_LEVEL_1,
  HOLDING_LEVEL_2,
  DELTA_HOLDING_LEVEL_2,
  PROTOCOL_PARAMETERS_SIZE
};

// Individual step within a protocol
struct ProtocolStep
{
  ampMode_t ampMode = VOLTAGE;
  stepType_t stepType = STEP;
  std::array<double,
             static_cast<size_t>(protocol_parameters::PROTOCOL_PARAMETERS_SIZE)>
      parameters {};
};  // struct ProtocolStep

// A segment within a protocol, made up of ProtocolSteps
struct ProtocolSegment
{
  std::vector<ProtocolStep> steps;
  size_t numSweeps = 1;
};

class Protocol
{
public:
  ProtocolSegment& getSegment(size_t seg_id);  // Return a segment
  size_t numSegments();  // Number of segments in a protocol
  size_t numSweeps(size_t seg_id);  // Number of sweeps in a segment
  void setSweeps(size_t seg_id, uint32_t sweeps);  // Set sweeps for a segment
  ProtocolStep& getStep(size_t segment,
                        size_t step);  // Return step in a segment
  size_t segmentSize(size_t seg_id);  // Return number of steps in segment
  void toDoc();  // Convert protocol to QDomDocument
  void fromDoc(const QDomDocument& doc);  // Load protocol from a QDomDocument
  void clear();  // Clears container

  void addSegment();  // Add a segment to container
  void deleteSegment(size_t seg_id);  // Delete a segment from container
  void modifySegment(size_t seg_id, const ProtocolSegment& segment);
  void addStep(size_t seg_id);  // Add a step to a segment in container
  void insertStep(size_t seg_id, size_t step_id);
  void deleteStep(size_t seg_id,
                  size_t step_id);  // Delete a step from segment in container
  void modifyStep(size_t seg_id, size_t step_id, const ProtocolStep& step);

  QDomDocument& getProtocolDoc() { return protocolDoc; }
  std::array<std::vector<double>, 2> dryrun(double period);

private:
  QDomElement segmentToNode(QDomDocument& doc, size_t seg_id);
  QDomElement stepToNode(QDomDocument& doc, size_t seg_id, size_t stepNum);
  QDomDocument protocolDoc;
  std::vector<ProtocolSegment> segments;
};  // class Protocol

struct protocol_state
{
  bool running = false;
  bool plotting = false;
  clamp_protocol::Protocol* protocol = nullptr;
};

class ClampProtocolWindow : public QWidget
{
  Q_OBJECT
public:
  explicit ClampProtocolWindow(QWidget* /*, Panel * */);
  void createGUI();

public slots:
  void addCurve(const std::vector<data_token_t>& data);

private slots:
  void setAxes();
  void clearPlot();
  void toggleOverlay();
  void togglePlotAfter();
  void changeColorScheme(int);

private:
  void colorCurves();

  BasicPlot* plot = nullptr;
  std::vector<QwtPlotCurve*>
      curveContainer;  // Used to hold curves to control memory allocation and
                       // deallocation
  std::array<std::vector<QVector<double>>, 2> curve_data;
  bool overlaySweeps;  // True: sweeps are plotted on same time scale
  bool plotAfter;  // True: only replot after a protocol has ended, False:
                   // replot after each step
  int colorScheme;  // 0: color by run, 1: color by trial, 2: color by sweep
  int runCounter;  // Used in run color scheme
  int sweepsShown;  // Used to keep track of sweeps shown in legend
  QFont font;

  QPixmap image0;
  QPixmap image1;

  QHBoxLayout* frameLayout = nullptr;
  QSpacerItem* spacer = nullptr;
  QGridLayout* layout1 = nullptr;
  QVBoxLayout* layout2 = nullptr;
  QVBoxLayout* layout3 = nullptr;

  QFrame* frame = nullptr;
  QLabel* currentScaleLabel = nullptr;
  QComboBox* currentScaleEdit = nullptr;
  QSpinBox* currentY2Edit = nullptr;
  QComboBox* timeScaleEdit = nullptr;
  QSpinBox* timeX2Edit = nullptr;
  QSpinBox* currentY1Edit = nullptr;
  QLabel* timeScaleLabel = nullptr;
  QSpinBox* timeX1Edit = nullptr;
  QPushButton* setAxesButton = nullptr;
  QCheckBox* overlaySweepsCheckBox = nullptr;
  QCheckBox* plotAfterCheckBox = nullptr;
  QLabel* textLabel1 = nullptr;
  QComboBox* colorByComboBox = nullptr;
  QPushButton* clearButton = nullptr;

  QMdiSubWindow* subWindow = nullptr;

signals:
  void emitCloseSignal();

};  // class ClampProtocolWindow

class ClampProtocolEditor : public QWidget
{
  Q_OBJECT

public:
  explicit ClampProtocolEditor(QWidget* parent);
  void createGUI();

public slots:
  QString loadProtocol();
  void loadProtocol(const QString&);
  void clearProtocol();
  void exportProtocol();
  void previewProtocol();
  void comboBoxChanged();
  virtual void protocolTable_currentChanged(int, int);
  virtual void protocolTable_verticalSliderReleased();

private slots:
  void addSegment();
  void deleteSegment();
  void addStep();
  void insertStep();
  void deleteStep();
  void updateSegment(QListWidgetItem*);
  void updateSegmentSweeps(int);
  void updateTableLabel();
  void updateTable();
  void updateStepAttribute(int, int);
  void updateStepType(int, stepType_t);
  void saveProtocol();
  void syncTableState(QTableWidgetItem* item);

private:
  void updateColumn(const ProtocolSegment& segment, int stepNum);
  int loadFileToProtocol(const QString&);
  bool protocolEmpty();

  Protocol protocol;  // Clamp protocol

  QGridLayout* layout2;
  QGroupBox* protocolDescriptionBox = nullptr;
  QGroupBox* segmentSummaryGroup = nullptr;
  QGroupBox* segmentSweepGroup = nullptr;
  QHBoxLayout* layout1 = nullptr;
  QHBoxLayout* layout4 = nullptr;
  QHBoxLayout* segmentSweepGroupLayout = nullptr;
  QLabel* segmentStepLabel = nullptr;
  QLabel* segmentSweepLabel = nullptr;
  QListWidget* segmentListWidget = nullptr;
  QPushButton* addSegmentButton = nullptr;
  QPushButton* addStepButton = nullptr;
  QPushButton* clearProtocolButton = nullptr;
  QPushButton* deleteSegmentButton = nullptr;
  QPushButton* deleteStepButton = nullptr;
  QPushButton* exportProtocolButton = nullptr;
  QPushButton* insertStepButton = nullptr;
  QPushButton* loadProtocolButton = nullptr;
  QPushButton* previewProtocolButton = nullptr;
  QPushButton* saveProtocolButton = nullptr;
  QSpinBox* segmentSweepSpinBox = nullptr;
  QTableWidget* protocolTable = nullptr;
  QVBoxLayout* layout3 = nullptr;
  QVBoxLayout* layout5 = nullptr;
  QVBoxLayout* layout6 = nullptr;
  QVBoxLayout* protocolDescriptionBoxLayout = nullptr;
  QVBoxLayout* segmentSummaryGroupLayout = nullptr;
  QVBoxLayout* windowLayout = nullptr;

  QMdiSubWindow* subWindow = nullptr;

  QStringList ampModeList;
  QStringList stepTypeList;

signals:
  void protocolTableScroll();
};

// Offset from parameter index in step struct to panel's row index
constexpr int param_2_row_offset = 2;

class Panel : public Widgets::Panel
{
  Q_OBJECT
public:
  Panel(QMainWindow* main_window, Event::Manager* ev_manager);
  void exec();
  void initParameters();
  void customizeGUI();

  void foreignToggleProtocol(bool);

  void receiveEvent(const ::Event::Object*);
  void receiveEventRT(const ::Event::Object*);

public slots:
  void loadProtocolFile();
  void openProtocolEditor();
  void openProtocolWindow();
  void updateProtocolWindow();
  void closeProtocolWindow();
  void closeProtocolEditor();
  void toggleProtocol();

signals:
  void plotCurve(std::vector<data_token_t> data);

private:
  std::list<ClampProtocolWindow*> plotWindowList;

  uint64_t trial=0;
  int64_t time=0;
  uint64_t sweep=1;
  uint64_t segmentNumber=1;
  int64_t intervalTime=1000;

  Protocol protocol;
  double stepOutput=0.0;
  double rampIncrement=0.0;
  RT::OS::Fifo* fifo = nullptr;
  std::vector<double> data;

  double prevSegmentEnd;  // Time segment ends after its first sweep
  int stepStart;  // Time when step starts divided by period

  bool recordData=false;
  bool protocolOn=false;
  bool recording=false;
  bool plotting=false;

  QTimer* plotTimer=nullptr;
  QCheckBox* recordCheckBox = nullptr;
  QLineEdit* loadFilePath = nullptr;
  QPushButton* loadButton = nullptr;
  QPushButton* editorButton = nullptr;
  QPushButton* viewerButton = nullptr;
  QPushButton* runProtocolButton = nullptr;
  ClampProtocolWindow* plotWindow = nullptr;
  ClampProtocolEditor* protocolEditor = nullptr;
};

class Component : public Widgets::Component
{
public:
  explicit Component(Widgets::Plugin* hplugin);
  void execute() override;

private:
  double getProtocolAmplitude(int64_t current_time);
  int segmentIdx;
  int sweepIdx;
  int stepIdx;
  int trialIdx;
  int numTrials;
  double voltage;
  double junctionPotential;
  double outputFactor;
  int64_t reference_time = 0;
  bool plotting = false;
  Protocol* protocol = nullptr;
  RT::OS::Fifo* fifo = nullptr;
};

class Plugin : public Widgets::Plugin
{
public:
  explicit Plugin(Event::Manager* ev_manager);
};

}  // namespace clamp_protocol
