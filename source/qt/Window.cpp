#include <iostream>
#include <cmath>
#include <QtGui>
#include <QSizePolicy>

#include "../config.h"
#include "GLWidget.h"
#include "Window.h"


Window::Window() {
	setlocale( LC_ALL, "C" );

	mGlWidget = new GLWidget;
	mMouseLastX = 0;
	mMouseLastY = 0;

	QAction *actionExit = new QAction( tr( "&Exit" ), this );
	actionExit->setShortcuts( QKeySequence::Quit );
	actionExit->setStatusTip( tr( "Quit the application." ) );
	connect( actionExit, SIGNAL( triggered() ), this, SLOT( close() ) );

	QMenu *menuFile = new QMenu( tr( "&File" ) );
	menuFile->addAction( actionExit );

	QMenuBar *menubar = new QMenuBar( this );
	menubar->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	menubar->addMenu( menuFile );

	mStatusBar = new QStatusBar( this );
	mStatusBar->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
	mStatusBar->showMessage( tr( "0 FPS" ) );

	QVBoxLayout *mainLayout = new QVBoxLayout();
	mainLayout->setSpacing( 0 );
	mainLayout->setMargin( 0 );
	mainLayout->addWidget( menubar );
	mainLayout->addWidget( mGlWidget );
	mainLayout->addWidget( mStatusBar );

	this->setLayout( mainLayout );
	this->setWindowTitle( tr( CFG_TITLE ) );
}


void Window::keyPressEvent( QKeyEvent *e ) {
	if( !mGlWidget->isRendering() ) {
		QWidget::keyPressEvent( e );
		return;
	}

	switch( e->key() ) {

		case Qt::Key_W:
			mGlWidget->cameraMoveForward();
			break;

		case Qt::Key_S:
			mGlWidget->cameraMoveBackward();
			break;

		case Qt::Key_A:
			mGlWidget->cameraMoveLeft();
			break;

		case Qt::Key_D:
			mGlWidget->cameraMoveRight();
			break;

		default:
			QWidget::keyPressEvent( e );

	}
}


void Window::mouseMoveEvent( QMouseEvent *e ) {
	if( e->buttons() == Qt::LeftButton && mGlWidget->isRendering() ) {
		int diffX = mMouseLastX - e->x();
		int diffY = mMouseLastY - e->y();

		mGlWidget->updateCamera( diffX, diffY );

		mMouseLastX = e->x();
		mMouseLastY = e->y();
	}
}


void Window::mousePressEvent( QMouseEvent *e ) {
	if( e->buttons() == Qt::LeftButton ) {
		mMouseLastX = e->x();
		mMouseLastY = e->y();
	}
}


void Window::updateStatus( const char *msg ) {
	mStatusBar->showMessage( tr( msg ) );
}
