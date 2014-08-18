#include "clamp-protocol-editor.h"
#include "protocol.h"
//#include "plot/basicplot.h"

#include <iostream>
/*#include <qwt-qt3/qwt_plot.h>
#include <qwt-qt3/qwt_plot_curve.h>
#include <qwt-qt3/qwt_text.h>*/
#include <QtGui>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_text.h>
#include <basicplot.h>

using namespace std;

/*ClampProtocol::CenterAlignTableItem::CenterAlignTableItem( QTable *table, EditType et = QTableItem::OnTyping): QTableItem(table, et) { // QTableItem subclass: used to make sure text is aligned in center }

int ClampProtocol::CenterAlignTableItem::alignment() const {
	return Qt::AlignHCenter | Qt::AlignVCenter;
}*/

ClampProtocolEditor::ClampProtocolEditor( QWidget * parent )/*: ProtocolEditorUI( parent, 0, false, Qt::WDestructiveClose ), currentSegmentNumber( 0 )*/ {

	createGUI();

	// QStringList for amp mode and step type combo boxes;
	ampModeList.append("Voltage");
	ampModeList.append("Current");
	stepTypeList.append("Step");
	stepTypeList.append("Ramp");
	stepTypeList.append("Train");
	//stepTypeList += "Custom";
	
	// Signal and slot connections for protocol editor UI
	QObject::connect( protocolTable, SIGNAL(selectionChanged()), this, SLOT(updateTableLabel(void)) );
	QObject::connect( addSegmentButton, SIGNAL(clicked(void)), this, SLOT(addSegment(void)) );
	QObject::connect( segmentListView, SIGNAL(activated(QModelIndex*)), this, SLOT(updateSegment(QModelIndex*)) );
	QObject::connect( segmentListView, SIGNAL(activated(QModelIndex*)), this, SLOT(updateTable(void)) );
//	QObject::connect( segmentListView, SIGNAL(selectionChanged(QListViewItem*)), this, SLOT(updateSegment(QListViewItem*)) );
//	QObject::connect( segmentListView, SIGNAL(selectionChanged(QListViewItem*)), this, SLOT(updateTable(void)) );
	QObject::connect( segmentSweepSpinBox,  SIGNAL(valueChanged(int)), this, SLOT(updateSegmentSweeps(int)) );
	QObject::connect( addStepButton, SIGNAL(clicked(void)), this, SLOT(addStep(void)) );
	QObject::connect( insertStepButton, SIGNAL(clicked(void)), this, SLOT(insertStep(void)) );
	QObject::connect( protocolTable, SIGNAL(valueChanged(int,int)), this, SLOT(updateStepAttribute(int,int)) );
	QObject::connect( deleteStepButton, SIGNAL(clicked(void)), this, SLOT(deleteStep(void)) );
	QObject::connect( deleteSegmentButton, SIGNAL(clicked(void)), this, SLOT(deleteSegment(void)) );
	QObject::connect( saveProtocolButton, SIGNAL(clicked(void)), this, SLOT(saveProtocol(void)) );
	QObject::connect( loadProtocolButton, SIGNAL(clicked(void)), this, SLOT(loadProtocol(void)) );
	QObject::connect( clearProtocolButton, SIGNAL(clicked(void)), this, SLOT(clearProtocol(void)) );
	QObject::connect( exportProtocolButton, SIGNAL(clicked(void)), this, SLOT(exportProtocol(void)) );
	QObject::connect( previewProtocolButton, SIGNAL(clicked(void)), this, SLOT(previewProtocol(void)) );
	
	resize( minimumSize() ); // Set window size to minimum
}

void ClampProtocolEditor::addSegment( void ) { // Adds another segment to protocol: listview, protocol container, and calls summary update
	if( !protocol.addSegment( currentSegmentNumber ) ) { // Protocol::addSegment returns 0 if it fails
		cout << "Error - ClampProtocolEditor::addSegment() failure" << endl;
		return ;
	}
	
	QString segmentName = "Segment ";
	if( protocol.numSegments() < 10) // To help with sorting, a zero prefix is used for single digits
		segmentName += "0";
	
	segmentName.append( QString( "%1" ).arg( protocol.numSegments() ) ); // Make QString of 'Segment ' + number of segments in container
	QModelIndex *element = new QModelIndex(segmentListView, segmentName); // Add segment reference to listView
//	QListVViewItem *element = new QListViewItem(segmentListView, segmentName); // Add segment reference to listView
	
	// Find newly inserted segment
	if( currentSegmentNumber + 1 < 10 )
		element = segmentListView->findItem( "Segment 0" + QString::number( currentSegmentNumber + 1 ), 0 );
	else
	element = segmentListView->findItem( "Segment " + QString::number( currentSegmentNumber + 1 ), 0 );
	
	if( !element ) { // element = 0 if nothing was found
		element =  segmentListView->lastItem();
		segmentListView->setCurrentItem( element );
	}
	else
	segmentListView->setCurrentItem( element ); // Focus on newly created segment
	
	updateSegment(element);
}

void ClampProtocolEditor::deleteSegment( void ) { // Deletes segment selected in listview: listview, protocol container, and calls summary update
	if( currentSegmentNumber == 0 ) {// If no segment exists, return and output error box
		QMessageBox::warning(this,
		"Error",
		"No segment has been created or selected.");
		return ;
	}
	
	// Message box asking for confirmation whether step should be deleted
	QString segmentString;
	segmentString.setNum(currentSegmentNumber);
	QString text = "Do you wish to delete Segment (" + segmentString + ")?"; // Text pointing out specific segment and step
	if( QMessageBox::question(this,
		"Delete Segment Confirmation",
	text,
	"Yes",
	"No") )
	return ; // Answer is no
	
	if( protocol.numSegments() == 1 ) { // If only 1 segment exists, clear protocol
		protocol.clear();
	}
	else {
		protocol.deleteSegment( currentSegmentNumber - 1 ); // - 1 since parameter is an index
	}
	
	segmentListView->clear(); // Clear list view
	
	// Rebuild list view
//	QListViewItem *element;
	QModelIndex *element;
	for( int i = 0; i < protocol.numSegments(); i++ ) {
		segmentString = "Segment ";
		if( i < 10 ) // Prefix with zero if a single digit
			segmentString += "0";
		segmentString += QString::number( i + 1 ); // Add number to segment string
		element = new QModelIndex( segmentListView, segmentString ); // Add segment to list view
//		element = new QListViewItem( segmentListView, segmentString ); // Add segment to list view
	}
	
	// Set to last segment and update table
	if( protocol.numSegments() > 0 ) {
		segmentListView->setCurrentItem( segmentListView->lastItem() );
		updateSegment( segmentListView->lastItem() );
		updateTable();
	}
	else { // No segments are left
		currentSegmentNumber = 0;
		protocolTable->setColumnCount( 0 ); // Clear table
		segmentSweepSpinBox->setValue( 0 ); // Set sweep number spin box to zero
	}
}

void ClampProtocolEditor::addStep( void ) { // Adds step to a protocol segment: updates protocol container
	if( currentSegmentNumber == 0 ) {// If no segment exists, return and output error box
		QMessageBox::warning( this,
		"Error",
		"No segment has been created or selected." );
		return ;
	}
	
	protocol.addStep( currentSegmentNumber - 1, protocolTable->columnCount() ); // Add step to segment
	
	updateTable(); // Rebuild table
	
	// Set scroll bar all the way to the right when step is added
	QScrollBar *hbar = protocolTable->horizontalScrollBar();
	hbar->setMaxValue(hbar->maxValue()+100); // Offset of 100 is due to race condition when scroll bar is actually updated
	hbar->setValue(hbar->maxValue());
}

void ClampProtocolEditor::insertStep( void ) { // Insert step to a protocol segment: updates protocol container
	if( currentSegmentNumber == 0 ) {// If no segment exists, return and output error box
		QMessageBox::warning( this,
		"Error",
		"No segment has been created or selected." );
		return ;
	}
	
	if( protocolTable->currentColumn() >= 0 ) // If other steps exist
		protocol.addStep( currentSegmentNumber - 1, protocolTable->currentColumn() + 1 ); // Add step to segment
	else // currentColumn() returns -1 if no columns exist
	protocol.addStep( currentSegmentNumber - 1, 0 ); // Add step to segment
	
	updateTable(); // Rebuild table
}

void ClampProtocolEditor::deleteStep( void ) { // Delete step from a protocol segment: updates table, listview, and protocol container
	if( currentSegmentNumber == 0 ) {// If no segment exists, return and output error box
		QMessageBox::warning( this,
		"Error",
		"No segment has been created or selected.");
		return ;
	}
	
	int stepNum = protocolTable->currentColumn(); // Step number that is currently selected
	
	if( stepNum == -1 ) {// If no step exists, return and output error box
		QMessageBox::warning( this,
		"Error",
		"No step has been created or selected." );
		return ;
	}
	
	// Message box asking for confirmation whether step should be deleted
	QString stepString;
	stepString.setNum( stepNum + 1 );
	QString segmentString;
	segmentString.setNum( currentSegmentNumber );
	QString text = "Do you wish to delete Step (" + stepString + ") of Segment (" + segmentString + ")?"; // Text pointing out specific segment and step
	bool answer =  QMessageBox::question(this,
	"Delete Step Confirmation",
	text,
	"Yes",
	"No");
	
	if (answer ) // No
		return ;
	else { // Yes
		protocol.deleteStep( currentSegmentNumber - 1, stepNum ); // Erase step from segment
		updateTable(); // Update table
	}
}

void ClampProtocolEditor::createStep( int stepNum ) { // Creates and initializes protocol step
	protocolTable->insertColumns( stepNum ); // Insert new column
	QHeader *horizontalHeader = protocolTable->horizontalHeader();
	QString headerLabel = "Step " + QString( "%1" ).arg( stepNum + 1 ); // Make header label
	horizontalHeader->setLabel( stepNum, headerLabel ); // Change column header to "Step #"
	
	QComboTableItem *comboItem;
	Step step = protocol.getStep( currentSegmentNumber - 1, stepNum );
	comboItem = new QComboTableItem( protocolTable, ampModeList ); // Amp mode combo box
	comboItem->setCurrentItem( step->retrieve(0) ); // Set box to retrieved attribute
	protocolTable->setItem( 0, stepNum, comboItem ); // Add amp mode combo box
	comboItem = new QComboTableItem( protocolTable, stepTypeList ); // Step type combo box
	comboItem = dynamic_cast<QComboTableItem*>( comboItem ); // Convert QTableItem* to QComboTableItem*
	comboItem->setCurrentItem( step->retrieve(1) ); // Set box to retrieved attribute
	protocolTable->setItem( 1, stepNum, comboItem ); // Add step type combo box
	comboItem = dynamic_cast<QComboTableItem*>( comboItem );
	
	QTableItem *item;
	QString text;
	// Due to alignment issues, all cells are manually set with CenterAlignTableItem
	// Sets each attribute to its correct valueable
	for( int i = 2; i <= 9; i++ ) {
		item = new ClampProtocol::CenterAlignTableItem( protocolTable );
		protocolTable->setItem( i, stepNum, item );
		text.setNum( step->retrieve(i) ); // Retrieve attribute value
		item->setText( text );
	}
	updateStepAttribute( 1, stepNum ); // Update column based on step type
}

void ClampProtocolEditor::updateSegment( /*QListViewItem*/ QModelIndex *segment ) { // Updates protocol description table when segment is clicked in listview
	// Update currentSegment to indicate which segment is selected
	QString label = segment->text( 0 ); // Grab label from selected item in listview
	label = label.right( 2 ); // Truncate label to get segment number
	currentSegmentNumber = label.toInt(); // Convert QString to int, now refers to current segment number
	segmentSweepSpinBox->setValue( protocol.numSweeps(currentSegmentNumber -1) ); // Set sweep number spin box to value stored for particular segment
	updateTableLabel(); // Update label of protocol table
}

void ClampProtocolEditor::updateSegmentSweeps( int sweepNum ) { // Update container that holds number of segment sweeps when spinbox value is changed
	protocol.setSweeps( currentSegmentNumber - 1, sweepNum ); // Set segment sweep value to spin box value
}

void ClampProtocolEditor::updateTableLabel( void ) { // Updates the label above protocol table to show current selected segment and step
	QString text = "Segment ";
	text.append( QString::number(currentSegmentNumber) );
	int col = protocolTable->currentColumn() + 1;
	if( col != 0 ) {
		text.append(": Step ");
		text.append( QString::number( col ) );
	}
	segmentStepLabel->setText(text);
}
void ClampProtocolEditor::updateTable( void ) { // Updates protocol description table: clears and reloads table from scratch
	protocolTable->setColumnCount( 0 ); // Clear table by setting columns to 0 *Note: deletes QTableItem objects*
	
	// Load steps from current clicked segment into protocol
	int i = 0;
	for( i = 0; i < protocol.numSteps( currentSegmentNumber - 1 ); i++ )
		createStep( i ); // Update step in protocol table
}

void ClampProtocolEditor::updateStepAttribute( int row, int col ) { // Updates protocol container when a table cell is changed
	Step step = protocol.getStep( currentSegmentNumber - 1, col );
	QComboTableItem *comboItem;
	QString text;
	bool check;
	
	// Check which row and update corresponding attribute in step container
	switch(row) {
		case 0:
		comboItem = dynamic_cast<QComboTableItem*>( protocolTable->item(row, col) );
		step->ampMode = (ProtocolStep::ampMode_t)comboItem->currentItem(); // Retrieve current item of combo box and set ampMode
		break;
		case 1:
		comboItem = dynamic_cast<QComboTableItem*>( protocolTable->item(row, col) );
		step->stepType = (ProtocolStep::stepType_t)comboItem->currentItem(); // Retrieve current item of combo box and set stepType
		updateStepType( col, step->stepType );
		break;
		case 2:
		text = protocolTable->text( row, col );
		step->stepDuration = text.toDouble( &check );
		break;
		case 3:
		text = protocolTable->text( row, col );
		step->deltaStepDuration = text.toDouble( &check );
		break;
		case 4:
		text = protocolTable->text( row, col );
		step->holdingLevel1 = text.toDouble( &check );
		break;
		case 5:
		text = protocolTable->text( row, col );
		step->deltaHoldingLevel1 = text.toDouble( &check );
		break;
		case 6:
		text = protocolTable->text( row, col );
		step->holdingLevel2 = text.toDouble( &check );
		break;
		case 7:
		text = protocolTable->text( row, col );
		step->deltaHoldingLevel2 = text.toDouble( &check );
		break;
		case 8:
		text = protocolTable->text( row, col );
		step->pulseWidth = text.toDouble( &check );
		break;
		case 9:
		text = protocolTable->text( row, col );
		step->pulseRate = text.toInt( &check );
		break;
		default:
		cout << "Error - ProtocolEditor::updateStepAttribute() - default case" << endl;
		break;
	}
	
	// Check to make sure entries are valid
	if( !check && row > 1 && text != "---" ) {
		if( row == 9 )
			QMessageBox::warning( this,
		"Error",
		"Pulse rate must be a whole number integer." );
		else
		QMessageBox::warning( this,
		"Error",
		"Step attribute is not a valid number." );
		
		protocolTable->setText( row, col, "0" );
	}
}

void ClampProtocolEditor::updateStepType( int stepNum, ProtocolStep::stepType_t stepType ) {
	// Disable unneeded attributes depending on step type
	// Enable needed attributes and set text to stored value
	Step step = protocol.getStep( currentSegmentNumber - 1, stepNum );
	QTableItem *item;
	
	switch( stepType ){
		case ProtocolStep::STEP:
		for( int i = 6; i <= 9; i++ ) {
			item = protocolTable->item( i, stepNum );
			item->setText( "---" );
			item->setEnabled( false );
			updateStepAttribute( i, stepNum );
		}
		for( int i = 2; i <= 5; i++ ) {
			item = protocolTable->item( i, stepNum );
			item->setText( QString::number( step->retrieve(i) ) ); // Retrieve attribute and set text
			item->setEnabled( true );
			updateStepAttribute( i, stepNum );
		}
		break;
		case ProtocolStep::RAMP:
		for(int i = 8; i <= 9; i++) {
			item = protocolTable->item( i, stepNum );
			item->setText( "---" );
			item->setEnabled( false );
			updateStepAttribute( i, stepNum );
		}
		for(int i = 2; i <= 7; i++) {
			item = protocolTable->item( i, stepNum );
			item->setText( QString::number( step->retrieve(i) ) ); // Retrieve attribute and set text
			item->setEnabled( true );
			updateStepAttribute( i, stepNum );
		}
		break;
		case ProtocolStep::TRAIN:
		for(int i = 2; i <= 7; i++) {
			item = protocolTable->item( i, stepNum );
			item->setText( "---" );
			item->setEnabled( false );
			updateStepAttribute( i, stepNum );
		}
		for(int i = 8; i <= 9; i++) {
			item = protocolTable->item( i, stepNum );
			item->setText( QString::number( step->retrieve(i) ) ); // Retrieve attribute and set text
			item->setEnabled( true );
			updateStepAttribute( i, stepNum );
		}
		break;
		/*case ProtocolStep::CUSTOM:
		for(int i = 2; i <= 9; i++) {
			item = protocolTable->item( i,stepNum );
			item->setText("---");
			item->setEnabled(false);
			updateStepAttribute( i, stepNum );
			
		}
		break;*/
	}
}

int ClampProtocolEditor::loadFileToProtocol( QString fileName ) { // Loads XML file of protocol data: updates table, listview, and protocol container
	// If protocol is present, warn user that protocol will be lost upon loading
	if( protocol.numSegments() != 0 &&
		QMessageBox::warning(
			this,
			"Load Protocol", "All unsaved changes to current protocol will be lost.\nDo you wish to continue?",
			QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes )
	return 0; // Return if answer is no
	
	QDomDocument doc( "protocol" );
	QFile file( fileName );
	
	if( !file.open( QIODevice::ReadOnly ) ) { // Make sure file can be opened, if not, warn user
		QMessageBox::warning(this,
		"Error",
		"Unable to open protocol file" );
		return 0;
	}
	if( !doc.setContent( &file ) ) { // Make sure file contents are loaded into document
		QMessageBox::warning(this,
		"Error",
		"Unable to set file contents to document" );
		file.close();
		return 0;
	}
	file.close();
	
	protocol.fromDoc( doc ); // Translate document into protocol
	
	if( protocol.numSegments() < 0 ) {
		QMessageBox::warning(this,
		"Error",
		"Protocol did not contain any segments" );
		return 0;
	}
	
	// Build segment listview
	for( int i = 0; i < protocol.numSegments(); i++ ) {
		QString segmentName = "Segment ";
		if( protocol.numSegments() < 10) // To help with sorting, a zero prefix is used for single digits
			segmentName += "0";
		segmentName += QString::number( i + 1 );
		
		QModelIndex *element = new QModelIndex(segmentListView, segmentName); // Add segment reference to listView
//		QListViewItem *element = new QListViewItem(segmentListView, segmentName); // Add segment reference to listView
	}
	
	segmentListView->setSelected( segmentListView->firstChild(), true );
	updateSegment( segmentListView->firstChild() );
	
	return 1;
}


QString ClampProtocolEditor::loadProtocol(void) {
	// Save dialog to retrieve desired filename and location
	QString fileName = QFileDialog::getOpenFileName(
	"~/",
	"Clamp Protocol Files (*.csp);;All Files(*.*)",
	this,
	"open file dialog",
	"Open a protocol" );
	if( fileName == NULL ) // Null if user cancels dialog
		return "";
	clearProtocol();
	int retval = loadFileToProtocol(fileName);
	
	if( !retval ) // If error occurs
		return "";
	
	return fileName;
}

void ClampProtocolEditor::loadProtocol(QString fileName) {
	loadFileToProtocol(fileName);
}

void ClampProtocolEditor::saveProtocol(void) { // Takes data within protocol container and converts to XML and saves to file
	if( protocolEmpty() ) // Exit if protocol is empty
		return ;
	
	protocol.toDoc(); // Update protocol QDomDocument
	
	// Save dialog to retrieve desired filename and location
	QString fileName = QFileDialog::getSaveFileName(
	"~/",
	"Clamp Protocol Files (*.csp);;All Files (*.*)",
	this,
	"save file dialog",
	"Save the protocol" );
	
	// If filename does not include .csp extension, add extension
	if( !( fileName.endsWith(".csp") ) )
		fileName.append( ".csp" );
	// If filename exists, warn user
	if ( QFileInfo( fileName ).exists() &&
		QMessageBox::warning(this,
			"File Exists", "Do you wish to overwrite " + fileName + "?",
			QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
	return ; // Return if answer is no
	
	// Save protocol to file
	QFile file( fileName ); // Open file
	if( !file.open(QIODevice::WriteOnly) ) { // Open file, return error if unable to do so
		QMessageBox::warning(this,
		"Error",
		"Unable to save file: Please check folder permissions." );
		return ;
	}
	QTextStream ts( &file ); // Open text stream
	ts << protocol.protocolDoc.toString(); // Write to file
	file.close(); // Close file
}

void ClampProtocolEditor::clearProtocol( void ) { // Clear protocol
	protocol.clear();
	currentSegmentNumber = 0;
	protocolTable->setColumnCount( 0 ); // Clear table
	segmentListView->clear();
	
	// Prevent resetting of spinbox from triggering slot function by disconnecting
	QObject::disconnect( segmentSweepSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateSegmentSweeps(int)) );
	segmentSweepSpinBox->setValue( 1 ); // Set sweep number spin box to zero
	QObject::connect( segmentSweepSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateSegmentSweeps(int)) );
}

void ClampProtocolEditor::exportProtocol( void ) { // Export protocol to a text file format ( time : output )
	if( protocolEmpty() ) // Exit if protocol is empty
		return ;
	
	// Grab period
	bool ok;
	double period = QInputDialog::getDouble(
	"Export Clamp Protocol", "Enter the period (ms): ", 0.010, 0,
	1000, 3, &ok, this );
	if( !ok ) // User cancels
		return ;
	
	// Save dialog to retrieve desired filename and location
	QString fileName = QFileDialog::getSaveFileName(
	"~/",
	"Text files (*.txt);;All Files (*.*)",
	this,
	"save file dialog",
	"Export Clamp Protocol" );
	
	// If filename does not include .txt extension, add extension
	if( !( fileName.endsWith(".txt") ) )
		fileName.append( ".txt" );
	// If filename exists, warn user
	if ( QFileInfo( fileName ).exists() &&
		QMessageBox::warning(
	this,
	"File Exists", "Do you wish to overwrite " + fileName + "?",
	QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
	return ; // Return if answer is no
	
	// Save protocol to file
	QFile file( fileName ); // Open file
	if( !file.open(QIODevice::WriteOnly) ) { // Open file, return error if unable to do so
		QMessageBox::warning(this,
		"Error",
		"Unable to save file: Please check folder permissions." );
		return ;
	}
	
	if( fileName == NULL ) // Null if user cancels dialog
		return ;
	
	// Run protocol with user specified period
	std::vector< std::vector<double> > run;
	run = protocol.run( period );
	std::vector<double> time = run.at( 0 );
	std::vector<double> output = run.at( 1 );
	
	QTextStream ts( &file );
	
	for( std::vector<double>::iterator itx = time.begin(), ity = output.begin();
		itx < time.end(); itx++, ity++ ) { // Iterate through vectors and output to file
		ts << *itx << " " << *ity << "\n";
	}
	
	file.close(); // Close file
}

void ClampProtocolEditor::previewProtocol( void ) { // Graph protocol output in a simple plot window
	if( protocolEmpty() ) // Exit if protocol is empty
		return ;
	
	// Create a dialog with a BasicPlot
	QDialog *dlg = new QDialog( parentWidget(), "Protocol Preview", false, Qt::WDestructiveClose );
	dlg->setCaption( "Protocol Preview" );
	QVBoxLayout *layout = new QVBoxLayout( dlg );
	QwtPlot *plot = new QwtPlot( dlg );
	layout->addWidget( plot );
	dlg->resize( 500, 500 );
	dlg->show();
	
	// Plot Settings
	plot->setCanvasBackground(QColor(70, 128, 186));
	QwtText xAxisTitle, yAxisTitle;
	xAxisTitle.setText( "Time (ms)" );
	yAxisTitle.setText( "Voltage (mV)" );
	plot->setAxisTitle( QwtPlot::xBottom, xAxisTitle );
	plot->setAxisTitle( QwtPlot::yLeft, yAxisTitle );
	plot->show();
	
	// Run protocol and plot
	std::vector< std::vector<double> > run;
	run = protocol.run( 0.01 );
	
	std::vector<double> timeVector, outputVector;
	timeVector = run.at( 0 );
	outputVector = run.at( 1 );
	
	QwtPlotCurve *curve = new QwtPlotCurve( "" ); // Will be deleted when preview window is closed
	curve->setData( &timeVector[0], &outputVector[0], timeVector.size() ); // Makes a hard copy of both time and output
	curve->attach( plot );
	plot->replot();
}

bool ClampProtocolEditor::protocolEmpty( void ) { // Make sure protocol has at least one segment with one step
	if( protocol.numSegments() == 0) { // Check if first segment exists
		QMessageBox::warning(this,
		"Error",
		"A protocol must contain at least one segment that contains at least one step" );
		return true;
	}
	else if( protocol.numSteps( 0 ) == 0 ) { // Check if first segment has a step
		QMessageBox::warning(this,
		"Error",
		"A protocol must contain at least one segment that contains at least one step" );
		return true;
	}
	
	return false;
}

void ClampProtocolEditor::createGUI(void) {
	QWidget::setAttribute(Qt::WA_DeleteOnClose);

	subWindow = new QMdiSubWindow;
	subWindow->setWindowFlags(Qt::CustomizeWingowHint);
	subWindow->setWindowFlags(Qt::WindowCloseButtonHint);
	subWindow->setOption(QMdiSubWindow::RubberBandResize, true);
	subWindow->setOption(QMdiSubWindow::RubberBandMove, true);
	ClampProtocol::getInstance()->createMdi(subWindow);

	windowLayout = new QVBoxLayout;
	this->setLayout(windowLayout);

	layout1 = new QHBoxLayout;
	saveProtocolButton = new QPushButton("Save");
	loadProtocolButton = new QPushButton("Load");
	exportProtocolButton = new QPushButton("Export");
	previewProtocolButton = new QPushButton("Preview");
	clearProtocolButton = new QPushButton("Clear");
	layout1->addWidget(saveProtocolButton);
	layout1->addWidget(loadProtocolButton);
	layout1->addWidget(exportProtocolButton);
	layout1->addWidget(previewProtocolButton);
	layout1->addWidget(clearProtocolButton);
	windowLayout->addLayout(layout1);

	layout2 = new QHBoxLayout;
	layout3 = new QHBoxLayout;

	protocolDescriptionBox = new QGroupBox("Protocol Description");
	protocolDescriptionBox->setAlignment(int(QGroupBox::AlignTop | Qt::AlignHCenter));
	protocolDescriptionBox->setcolumnLayout(0, Qt::Vertical);
	protocolDescriptionBoxLayout = new QVBoxLayout;
	protocolDescriptionBox->setLayout(protocolDescriptionBoxLayout);
	protocolDescriptionBoxLayout->setAlignment(Qt::AlignTop);

	segmentStepLabel = new QLabel("Step");
	segmentStepLabel->setAlignment(QLabel::AlignCenter);
	protocolDescriptionBoxLayout->addWidget(segmentStepLabel);

	protocolTable = new QTable;
	protocolDescriptionBoxLayout->addWidget(protocolTable);
	protocolTable->setRowCount(protocolTable->rowCount() + 1);
	protocolTable->verticalHeader()->setLabel(protocolTable->rowCount() - 1, "Amplifier Mode");
	protocolTable->setRowCount(protocolTable->rowCount() + 1);
	protocolTable->verticalHeader()->setLabel(protocolTable->rowCount() - 1, "Step Type");
	protocolTable->setRowCount(protocolTable->rowCount() + 1);
	protocolTable->verticalHeader()->setLabel(protocolTable->rowCount() - 1, "Step Duration");
	protocolTable->setRowCount(protocolTable->rowCount() + 1);
	protocolTable->verticalHeader()->setLabel(protocolTable->rowCount() - 1, "\xce\x94\x20\x53\x74\x65\x70\x20\x44\x75\x72\x61\x74\x69\x6f\x6e\x20\x28\x6d\x73\x29");
	protocolTable->setRowCount(protocolTable->rowCount() + 1);
	protocolTable->verticalHeader()->setLabel(protocolTable->rowCount() - 1, "Holding Level 1");
	protocolTable->setRowCount(protocolTable->rowCount() + 1);
	protocolTable->verticalHeader()->setLabel(protocolTable->rowCount() - 1, "\xce\x94\x20\x48\x6f\x6c\x64\x69\x6e\x67\x20\x4c\x65\x76\x65\x6c\x20\x31\x20\x28\x6d\x56\x2f\x70\x41\x29");
	protocolTable->setRowCount(protocolTable->rowCount() + 1);
	protocolTable->verticalHeader()->setLabel(protocolTable->rowCount() - 1, "Holding Level 2");
	protocolTable->setRowCount(protocolTable->rowCount() + 1);
	protocolTable->verticalHeader()->setLabel(protocolTable->rowCount() - 1, "\xce\x94\x20\x48\x6f\x6c\x64\x69\x6e\x67\x20\x4c\x65\x76\x65\x6c\x20\x32\x20\x28\x6d\x56\x2f\x70\x41\x29");
	protocolTable->setRowCount(protocolTable->rowCount() + 1);
	protocolTable->verticalHeader()->setLabel(protocolTable->rowCount() - 1, "Pulse Width (ms)");
	protocolTable->setRowCount(protocolTable->rowCount() + 1);
	protocolTable->verticalHeader()->setLabel(protocolTable->rowCount() - 1, "Pulse Train Rate");
	protocolTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	protocolTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	protocolTable->setRowCount(10);
	protocolTable->setColumnCount(0);
	protocolTable->setSelectionMode(QTableWidget::Single);
	protocolDescriptionBoxLayout->addWidget(protocolTable);
	layout3->addWidget(protocolDescriptionBox);

	layout4 = new QHBoxLayout;
	addStepButton = new QPushButton("Add Step");
	insertStepButton = new QPushButton("Insert Step");
	deleteStepButton = new QPushButton("Delete Step");
	layout4->addWidget(addStepButton);
	layout4->addWidget(insertStepButton);
	layout4->addWidget(deleteStepButton);

	layout3->addLayout(layout4);
	layout2->addLayout(layout3);

	layout5 = new QVBoxLayout;
	segmentSummaryGroup = new QGroupBox("Segment Summary");
	segmentSummaryGroup->setAlignment(Qt::AlignHCenter);
	segmentSummaryGroup->setColumnLayout(0, Qt::Vertical);
	segmentSummaryGroupLayout = new QVBoxLayout;
	segmentSummaryGroup->setLayout(segmentSummaryGroupLayout);
	segmentSummaryGroupLayout->setAlignment(Qt::AlignTop);

	segmentSweepGroup = new QGroupBox("Segment Sweeps");
	segmentSweepGroupLayout = new QHBoxLayout;
	segmentSweepGroup->setLayout(segmentSweepGroupLayout);
	segmentSweepGroupLayout->setAlignment(Qt::AlignTop);

	segmentSweepLabel = new QLabel("Segment Sweeps");
	segmentSweepSpinBox = new QSpinBox;
	segmentSweepGroupLayout->addWidget(segmentSweepLabel);
	segmentSweepGroupLayout->addWidget(segmentSweepSpinBox);

	segmentSummaryGroupLayout->addWidget(segmentSweepGroup);

	segmentListView = new QListView;
	segmentListView->addColumns("Segment #");
	segmentListView->header()->setResizeEnabled(FALSE, segmentListView->header()->count()-1);
	segmentListView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	segmentListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	segmentListView->setResizeMode(QListView::LastColumn);
	segmentListView->setTreeStepSize(20);
	segmentSummaryGroupLayout->addWidget(segmentListView);
	layout5->addWidget(segmentSummaryGroup);

	layout6 = new QHBoxLayout;
	addSegmentButton = new QPushButton("Add Segment");
	deleteSegmentButton = new QPushButton("Delete Segment");
	layout6->addWidget(addSegmentButton);
	layout6->addWidget(deleteSegmentButton);
	layout5->addLayout(layout6);
	layout2->addLayout(layout5);
	windowLayout->addLayout(layout2);
	clearWState(WState_Polished);
}
