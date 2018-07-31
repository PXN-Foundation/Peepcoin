/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "addresstablemodel.h"
#include "transactionview.h"
#include "overviewpage.h"
#include "bitcoinunits.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"
#include "notificator.h"
#include "guiutil.h"
#include "rpcconsole.h"
#include "wallet.h"
#include "mintingview.h"
#include "blockbrowser.h"
#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QStackedWidget>
#include <QDateTime>
#include <QFileDialog>
#include <QDesktopServices>
#include <QTimer>
#include <QDragEnterEvent>
#include <QUrl>
#include <QStyle>
#include <iostream>
#include <QDebug>
extern CWallet* pwalletMain;
extern int64_t nLastCoinStakeSearchInterval;
double GetPoSKernelPS();

BitcoinGUI::BitcoinGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    encryptWalletAction(0),
    changePassphraseAction(0),
    unlockWalletAction(0),
    lockWalletAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0),
    nWeight(0),
	spinnerFrame(0)
{
	setFixedSize(1000, 542);
	setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
	setMinimumSize(0,0);
    setWindowTitle(tr("Peepcoin") + " - " + tr("Wallet"));
#ifdef Q_OS_LINUX
	qApp->setStyleSheet("QMainWindow { background-image:url(:images/bkg2);border:none;font-family:'Open Sans,sans-serif'; }");
#else
	qApp->setStyleSheet("QMainWindow { border-image:url(:images/bkg);border:none;font-family:'Open Sans,sans-serif'; }");
#endif
#ifndef Q_OS_MAC
    qApp->setWindowIcon(QIcon(":icons/bitcoin"));
    setWindowIcon(QIcon(":icons/bitcoin"));
#else
    setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
    // Accept D&D of URIs
    setAcceptDrops(true);
    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create the tray icon (or setup the dock icon)
    createTrayIcon();

    // Create tabs
    overviewPage = new OverviewPage();

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    transactionView = new TransactionView(this);
    vbox->addWidget(transactionView);
    transactionsPage->setLayout(vbox);

    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);

    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);

    sendCoinsPage = new SendCoinsDialog(this);

    signVerifyMessageDialog = new SignVerifyMessageDialog(this);

    centralWidget = new QStackedWidget(this);
    centralWidget->addWidget(overviewPage);
    centralWidget->addWidget(transactionsPage);
    centralWidget->addWidget(addressBookPage);
    centralWidget->addWidget(receiveCoinsPage);
    centralWidget->addWidget(sendCoinsPage);
    setCentralWidget(centralWidget);


    mintingPage = new QWidget(this);
    QVBoxLayout *vboxMinting = new QVBoxLayout();
    mintingView = new MintingView(this);
    vboxMinting->addWidget(mintingView);
    mintingPage->setLayout(vboxMinting);
    centralWidget->addWidget(mintingPage);

    blockExplorer = new BlockBrowser(this);
    centralWidget->addWidget(blockExplorer);

    // Create status bar
    statusBar();

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    labelEncryptionIcon = new QLabel();
    labelStakingIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelEncryptionIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelStakingIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    if (GetBoolArg("-staking", true))
    {
        QTimer *timerStakingIcon = new QTimer(labelStakingIcon);
        connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(updateStakingIcon()));
        timerStakingIcon->start(30 * 1000);
        updateStakingIcon();
    }

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new QProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = qApp->style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    // Clicking on "Verify Message" in the address book sends you to the verify message tab
    connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));
    // Clicking on "Sign Message" in the receive coins page sends you to the sign message tab
    connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));

    gotoOverviewPage();
}

BitcoinGUI::~BitcoinGUI()
{
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
#endif
}

void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setToolTip(tr("Show general overview of wallet"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send coins"), this);
    sendCoinsAction->setToolTip(tr("Send coins to a Peepcoin address"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive coins"), this);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), this);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(addressBookAction);


    // Minting View
    mintingViewAction = new QAction(QIcon(":/icons/stake"), tr("&Staking"), this);
    mintingViewAction->setToolTip(tr("Shows staking details"));
    mintingViewAction->setCheckable(true);
    mintingViewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    tabGroup->addAction(mintingViewAction);

    connect(mintingViewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(mintingViewAction, SIGNAL(triggered()), this, SLOT(gotoMintingPage()));

    // Blockexplorer View

    blockexplorerAction = new QAction(QIcon(":/icons/block"), tr("&Explorer"), this);
    blockexplorerAction->setToolTip(tr("Gives Information about blocks and transactions"));
    blockexplorerAction->setCheckable(true);
    blockexplorerAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_7));
    tabGroup->addAction(blockexplorerAction);

    connect(blockexplorerAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(blockexplorerAction, SIGNAL(triggered()), this, SLOT(gotoBlockexplorerPage()));


    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setToolTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
	faqAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Wallet FAQ"), this);
    faqAction->setToolTip(tr("Peepcoin Wallet FAQ"));
	faq2Action = new QAction(QIcon(":/icons/staking_on"), tr("&Staking FAQ"), this);
    faq2Action->setToolTip(tr("Peepcoin Staking FAQ"));
	faq3Action = new QAction(QIcon(":/icons/bitcoin"), tr("&General FAQ"), this);
    faq3Action->setToolTip(tr("Peepcoin General FAQ"));
	whitepaperAction = new QAction(QIcon(":/icons/bitcoin"), tr("&DAPS Whitepaper"), this);
    whitepaperAction->setToolTip(tr("DAPS Whitepaper"));
	swapAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Swap Info"), this);
    swapAction->setToolTip(tr("Swap Info"));
    aboutAction = new QAction(QIcon(":/icons/bitcoin"), tr("&About Peepcoin"), this);
    aboutAction->setToolTip(tr("Show information about Peepcoin"));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutQtAction = new QAction(QIcon(":/icons/about_qt"), tr("About &Qt"), this);
    aboutQtAction->setToolTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for Peepcoin"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Show / Hide"), this);
    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setToolTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setToolTip(tr("Change the passphrase used for wallet encryption"));
    unlockWalletAction = new QAction(QIcon(":/icons/lock_open"), tr("&Unlock Wallet..."), this);
    unlockWalletAction->setToolTip(tr("Unlock wallet"));
    lockWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Lock Wallet"), this);
    lockWalletAction->setToolTip(tr("Lock wallet"));
    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);
	
	bleuBTCAction = new QAction(QIcon(":/icons/bleutrade"), tr("&Bleutrade - PCN/BTC"), this);
    bleuBTCAction->setToolTip(tr("Buy Peepcoin with BTC on Bleutrade"));
	bleuDOGEAction = new QAction(QIcon(":/icons/bleutrade"), tr("&Bleutrade - PCN/DOGE"), this);
    bleuDOGEAction->setToolTip(tr("Buy Peepcoin with DOGE on Bleutrade"));
	bleuETHAction = new QAction(QIcon(":/icons/bleutrade"), tr("&Bleutrade - PCN/ETH"), this);
	bleuETHAction->setToolTip(tr("Buy Peepcoin with ETH on Bleutrade"));
    bleuUSDTAction = new QAction(QIcon(":/icons/bleutrade"), tr("&Bleutrade - PCN/USDT"), this);
    bleuUSDTAction->setToolTip(tr("Buy Peepcoin with USDT on Bleutrade"));
	tradeOgreAction = new QAction(QIcon(":/icons/tradeogre"), tr("&TradeOgre - PCN/BTC"), this);
    tradeOgreAction->setToolTip(tr("Buy Peepcoin for BTC on TradeOgre"));
	cryptoHubAction = new QAction(QIcon(":/icons/cryptohub"), tr("&CryptoHub - PCN/BTC"), this);
    cryptoHubAction->setToolTip(tr("Buy Peepcoin for BTC on CryptoHub"));
	cpatexBTCAction = new QAction(QIcon(":/icons/cpatex"), tr("&C-Patex - PCN/BTC"), this);
    cpatexBTCAction->setToolTip(tr("Buy Peepcoin for BTC on C-Patex"));
	cpatexDOGEAction = new QAction(QIcon(":/icons/cpatex"), tr("&C-Patex - PCN/DOGE"), this);
	cpatexDOGEAction->setToolTip(tr("Buy Peepcoin for DOGE on C-Patex"));
	cryptopiaAction = new QAction(QIcon(":/icons/cryptopia"), tr("Cryptopia - Coming Soon"), this);
	cryptopiaAction->setToolTip(tr("Buy Peepcoin for BTC on Cryptopia - Coming SOON"));
	cryptobridgeAction = new QAction(QIcon(":/icons/cryptobridge"), tr("&CryptoBridge - Coming Soon"), this);
	cryptobridgeAction->setToolTip(tr("Buy Peepcoin for BTC on CryptoBridge - Coming Soon"));
	otherExchangesAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Other Exchanges"), this);
	otherExchangesAction->setToolTip(tr("Other Exchanges currently listing PeepCoin"));

	facebookAction = new QAction(QIcon(":/icons/facebook"), tr("Facebook"), this);
    facebookAction->setToolTip(tr("DAPS Facebook"));
	twitterAction = new QAction(QIcon(":/icons/twitter"), tr("Twitter"), this);
    twitterAction->setToolTip(tr("DAPS Twitter"));
	discordAction = new QAction(QIcon(":/icons/discord"), tr("Discord"), this);
    discordAction->setToolTip(tr("DAPS Discord"));
	telegramAction = new QAction(QIcon(":/icons/telegram"), tr("Telegram - Main"), this);
    telegramAction->setToolTip(tr("DAPS Telegram - Main"));
	telegram2Action = new QAction(QIcon(":/icons/telegram"), tr("Telegram - Announcements"), this);
    telegram2Action->setToolTip(tr("DAPS Telegram - Announcements"));
	telegram3Action = new QAction(QIcon(":/icons/telegram"), tr("Telegram - How To"), this);
    telegram3Action->setToolTip(tr("DAPS Telegram - How To"));
	telegram4Action = new QAction(QIcon(":/icons/telegram"), tr("Telegram - Moon Spam"), this);
    telegram4Action->setToolTip(tr("DAPS Telegram - Moon Spam"));
	mediumAction = new QAction(QIcon(":/icons/medium"), tr("Medium"), this);
    mediumAction->setToolTip(tr("DAPS Medium"));
	steemitAction = new QAction(QIcon(":/icons/steemit"), tr("Steemit"), this);
    steemitAction->setToolTip(tr("DAPS Steemit"));
	instagramAction = new QAction(QIcon(":/icons/instagram"), tr("Instagram"), this);
    instagramAction->setToolTip(tr("DAPS Instagram"));
	redditAction = new QAction(QIcon(":/icons/reddit"), tr("Reddit"), this);
    redditAction->setToolTip(tr("DAPS Reddit"));


    exportAction = new QAction(QIcon(":/icons/export"), tr("&Export..."), this);
    exportAction->setToolTip(tr("Export the data in the current tab to a file"));
    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setToolTip(tr("Open debugging and diagnostic console"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(faqAction, SIGNAL(triggered()), this, SLOT(faqClicked()));
    connect(faq2Action, SIGNAL(triggered()), this, SLOT(faq2Clicked()));
    connect(faq3Action, SIGNAL(triggered()), this, SLOT(faq3Clicked()));
    connect(swapAction, SIGNAL(triggered()), this, SLOT(swapClicked()));
    connect(whitepaperAction, SIGNAL(triggered()), this, SLOT(whitepaperClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(unlockWalletAction, SIGNAL(triggered()), this, SLOT(unlockWallet()));
    connect(lockWalletAction, SIGNAL(triggered()), this, SLOT(lockWallet()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
	
	connect(bleuBTCAction, SIGNAL(triggered()), this, SLOT(bleuBTCClicked()));
	connect(bleuDOGEAction, SIGNAL(triggered()), this, SLOT(bleuDOGEClicked()));
    connect(bleuETHAction, SIGNAL(triggered()), this, SLOT(bleuETHClicked()));
	connect(bleuUSDTAction, SIGNAL(triggered()), this, SLOT(bleuUSDTClicked()));
	connect(tradeOgreAction, SIGNAL(triggered()), this, SLOT(tradeOgreClicked()));
	connect(cryptoHubAction, SIGNAL(triggered()), this, SLOT(cryptoHubClicked()));
	connect(cpatexBTCAction, SIGNAL(triggered()), this, SLOT(cpatexBTCClicked()));
	connect(cpatexDOGEAction, SIGNAL(triggered()), this, SLOT(cpatexDOGEClicked()));
	connect(otherExchangesAction, SIGNAL(triggered()), this, SLOT(otherExchangesClicked()));

	connect(facebookAction, SIGNAL(triggered()), this, SLOT(facebookActionClicked()));
	connect(twitterAction, SIGNAL(triggered()), this, SLOT(twitterActionClicked()));
	connect(discordAction, SIGNAL(triggered()), this, SLOT(discordActionClicked()));
	connect(telegramAction, SIGNAL(triggered()), this, SLOT(telegramActionClicked()));
	connect(telegram2Action, SIGNAL(triggered()), this, SLOT(telegram2ActionClicked()));
	connect(telegram3Action, SIGNAL(triggered()), this, SLOT(telegram3ActionClicked()));
	connect(telegram4Action, SIGNAL(triggered()), this, SLOT(telegram4ActionClicked()));
	connect(mediumAction, SIGNAL(triggered()), this, SLOT(mediumActionClicked()));
	connect(steemitAction, SIGNAL(triggered()), this, SLOT(steemitActionClicked()));
	connect(instagramAction, SIGNAL(triggered()), this, SLOT(instagramActionClicked()));
	connect(redditAction, SIGNAL(triggered()), this, SLOT(redditActionClicked()));

}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->addAction(backupWalletAction);
    file->addAction(exportAction);
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addSeparator();
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);
    settings->addAction(unlockWalletAction);
    settings->addAction(lockWalletAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

	QMenu *exchanges = appMenuBar->addMenu(tr("Exchanges"));
	exchanges->addAction(bleuBTCAction);
	exchanges->addAction(bleuDOGEAction);
	exchanges->addAction(bleuETHAction);
	exchanges->addAction(bleuUSDTAction);	
	exchanges->addSeparator();
	exchanges->addAction(tradeOgreAction);
    exchanges->addSeparator();
	exchanges->addAction(cryptoHubAction);
    exchanges->addSeparator();
    exchanges->addAction(cpatexBTCAction);
    exchanges->addAction(cpatexDOGEAction);
	exchanges->addSeparator();
	exchanges->addAction(otherExchangesAction);
    /*
	QMenu *socials = appMenuBar->addMenu(tr("Social"));
	socials->addAction(facebookAction);
	socials->addAction(twitterAction);
	socials->addAction(discordAction);
	socials->addAction(telegramAction);
	socials->addAction(telegram2Action);
	socials->addAction(telegram3Action);
	socials->addAction(telegram4Action);
	socials->addAction(mediumAction);
    socials->addAction(steemitAction);
    socials->addAction(instagramAction);
	socials->addAction(redditAction);
    */
    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
	help->addSeparator();
/*	help->addAction(faqAction);
	help->addAction(faq2Action);
	help->addAction(faq3Action);
	help->addSeparator();
	help->addAction(whitepaperAction);
    help->addAction(swapAction);*/
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
	
}

void BitcoinGUI::createToolBars()
{
    QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
#ifdef Q_OS_LINUX
    //Linux - Set Purple tabs, white text, tooltips unaffected
    toolbar->setStyleSheet("background-color: #47146C; color: white");
#else
    //Windows/Mac - Set Purple tabs, white text, tooltips adjusted
    toolbar->setStyleSheet(" QToolTip { color: black; background-color: #F9F8C4}; color: white; background-color: #47146C");
#endif
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);
    toolbar->addAction(mintingViewAction);
    toolbar->addAction(blockexplorerAction);

    QToolBar *toolbar2 = addToolBar(tr("Actions toolbar"));
    toolbar2->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
#ifdef Q_OS_LINUX
    //Linux - Set Purple tabs, white text, tooltips unaffected
    toolbar2->setStyleSheet("background-color: #47146C");
#else
    //Windows/Mac - Set Purple tabs, tooltips adjusted
    toolbar2->setStyleSheet(" QToolTip { color: black; background-color: #F9F8C4}; background-color: #47146C");
#endif
    toolbar2->addAction(exportAction);
}

void BitcoinGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        // Replace some strings and icons, when using the testnet
        if(clientModel->isTestNet())
        {
            setWindowTitle(windowTitle() + QString(" ") + tr("[testnet]"));
#ifndef Q_OS_MAC
            qApp->setWindowIcon(QIcon(":icons/bitcoin_testnet"));
            setWindowIcon(QIcon(":icons/bitcoin_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/bitcoin_testnet"));
#endif
            if(trayIcon)
            {
                trayIcon->setToolTip(tr("Peepcoin client") + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
                toggleHideAction->setIcon(QIcon(":/icons/toolbar_testnet"));
            }

            aboutAction->setIcon(QIcon(":/icons/toolbar_testnet"));
        }

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(setNumBlocks(int,int)));

        // Report errors from network/worker thread
        connect(clientModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        rpcConsole->setClientModel(clientModel);
        addressBookPage->setOptionsModel(clientModel->getOptionsModel());
        receiveCoinsPage->setOptionsModel(clientModel->getOptionsModel());
    }
}

void BitcoinGUI::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    if(walletModel)
    {
        // Report errors from wallet thread
        connect(walletModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        // Put transaction list in tabs
        transactionView->setModel(walletModel);
        mintingView->setModel(walletModel);
        overviewPage->setModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        signVerifyMessageDialog->setModel(walletModel);

        setEncryptionStatus(walletModel->getEncryptionStatus());
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(setEncryptionStatus(int)));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));
    }
}

void BitcoinGUI::createTrayIcon()
{
    QMenu *trayIconMenu;
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip(tr("Peepcoin client"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif

    notificator = new Notificator(qApp->applicationName(), trayIcon, this);
}

#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

void BitcoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void BitcoinGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

void BitcoinGUI::faqClicked()
{
	QDesktopServices::openUrl(QUrl("https://www.dapscoin.com/wallets"));
}

void BitcoinGUI::faq2Clicked()
{
	QDesktopServices::openUrl(QUrl("https://www.dapscoin.com/staking"));
}

void BitcoinGUI::faq3Clicked()
{
	QDesktopServices::openUrl(QUrl("https://dapscoin.com/roadmap/#faq"));
}

void BitcoinGUI::swapClicked()
{
	QDesktopServices::openUrl(QUrl("https://dapscoin.com/swap"));
}

void BitcoinGUI::whitepaperClicked()
{
	QDesktopServices::openUrl(QUrl("https://dapscoin.com/whitepaper.pdf"));
}

void BitcoinGUI::bleuBTCClicked()
{
	QDesktopServices::openUrl(QUrl("https://bleutrade.com/exchange/PCN/BTC"));
}

void BitcoinGUI::bleuDOGEClicked()
{
	QDesktopServices::openUrl(QUrl("https://bleutrade.com/exchange/PCN/DOGE"));
}

void BitcoinGUI::bleuETHClicked()
{
	QDesktopServices::openUrl(QUrl("https://bleutrade.com/exchange/PCN/ETH"));
}

void BitcoinGUI::bleuUSDTClicked()
{
	QDesktopServices::openUrl(QUrl("https://bleutrade.com/exchange/PCN/USDT"));
}

void BitcoinGUI::tradeOgreClicked()
{
	QDesktopServices::openUrl(QUrl("https://tradeogre.com/exchange/BTC-PCN"));
}

void BitcoinGUI::cryptoHubClicked()
{
	QDesktopServices::openUrl(QUrl("https://cryptohub.online/market/PCN/"));
}

void BitcoinGUI::cpatexBTCClicked()
{
	QDesktopServices::openUrl(QUrl("https://c-patex.com/markets/pcnbtc"));
}

void BitcoinGUI::cpatexDOGEClicked()
{
	QDesktopServices::openUrl(QUrl("https://c-patex.com/markets/pcndoge"));
}

void BitcoinGUI::otherExchangesClicked()
{
	QDesktopServices::openUrl(QUrl("https://www.dapscoin.com/markets"));
}

void BitcoinGUI::facebookActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://www.facebook.com/dapscoinofficial/"));
}
void BitcoinGUI::twitterActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://twitter.com/DAPScoin"));
}
void BitcoinGUI::discordActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://discord.gg/w898czA"));
}
void BitcoinGUI::telegramActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://t.me/dapscoin"));
}
void BitcoinGUI::telegram2ActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://t.me/DAPSUpdates"));
}
void BitcoinGUI::telegram3ActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://t.me/HOW_TO_DAPS"));
}
void BitcoinGUI::telegram4ActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://t.me/DAPS_MOONSPAM"));
}
void BitcoinGUI::mediumActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://medium.com/dapscoinofficial"));
}
void BitcoinGUI::instagramActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://www.instagram.com/official_dapscoin/"));
}
void BitcoinGUI::redditActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://www.reddit.com/r/DAPSCoin/"));
}
void BitcoinGUI::steemitActionClicked()
{
	QDesktopServices::openUrl(QUrl("https://steemit.com/@dapscoin/"));
}


void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to Peepcoin network", "", count));
}

void BitcoinGUI::setNumBlocks(int count, int nTotalBlocks)
{
    // don't show / hide progress bar and its label if we have no connection to the network
    if (!clientModel || clientModel->getNumConnections() == 0)
    {
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);

        return;
    }

    QString strStatusBarWarnings = clientModel->getStatusBarWarnings();
    QString tooltip;

    if(count < nTotalBlocks)
    {
        int nRemainingBlocks = nTotalBlocks - count;
        float nPercentageDone = count / (nTotalBlocks * 0.01f);

        if (strStatusBarWarnings.isEmpty())
        {
            progressBarLabel->setText(tr("Synchronizing with network..."));
            progressBarLabel->setVisible(true);
            progressBar->setFormat(tr("~%n block(s) remaining", "", nRemainingBlocks));
            progressBar->setMaximum(nTotalBlocks);
            progressBar->setValue(count);
            progressBar->setVisible(true);
        }

        tooltip = tr("Downloaded %1 of %2 blocks of transaction history (%3% done).").arg(count).arg(nTotalBlocks).arg(nPercentageDone, 0, 'f', 2);
    }
    else
    {
        if (strStatusBarWarnings.isEmpty())
            progressBarLabel->setVisible(false);

        progressBar->setVisible(false);
        tooltip = tr("Downloaded %1 blocks of transaction history.").arg(count);
    }

    // Override progressBarLabel text and hide progress bar, when we have warnings to display
    if (!strStatusBarWarnings.isEmpty())
    {
        progressBarLabel->setText(strStatusBarWarnings);
        progressBarLabel->setVisible(true);
        progressBar->setVisible(false);
    }

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int secs = lastBlockDate.secsTo(QDateTime::currentDateTime());
    QString text;

    // Represent time from last generated block in human readable text
    if(secs <= 0)
    {
        // Fully up to date. Leave text empty.
    }
    else if(secs < 60)
    {
        text = tr("%n second(s) ago","",secs);
    }
    else if(secs < 60*60)
    {
        text = tr("%n minute(s) ago","",secs/60);
    }
    else if(secs < 24*60*60)
    {
        text = tr("%n hour(s) ago","",secs/(60*60));
    }
    else
    {
        text = tr("%n day(s) ago","",secs/(60*60*24));
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        overviewPage->showOutOfSyncWarning(false);
    }
    else
    {
        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
		{
            labelBlocksIcon->setPixmap(QIcon(QString(
                ":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0')))
                .pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }


        overviewPage->showOutOfSyncWarning(true);
    }

    if(!text.isEmpty())
    {
        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1.").arg(text);
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BitcoinGUI::error(const QString &title, const QString &message, bool modal)
{
    // Report errors from network/worker thread
    if(modal)
    {
        QMessageBox::critical(this, title, message, QMessageBox::Ok, QMessageBox::Ok);
    } else {
        notificator->notify(Notificator::Critical, title, message);
    }
}

void BitcoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent *event)
{
    if(clientModel)
    {
#ifndef Q_OS_MAC // Ignored on Mac
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
           !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            qApp->quit();
        }
#endif
    }
    QMainWindow::closeEvent(event);
}

void BitcoinGUI::askFee(qint64 nFeeRequired, bool *payFee)
{
    QString strMessage =
        tr("This transaction is over the size limit.  You can still send it for a fee of %1, "
          "which goes to the nodes that process your transaction and helps to support the network.  "
          "Do you want to pay the fee?").arg(
                BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
          this, tr("Confirm transaction fee"), strMessage,
          QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

void BitcoinGUI::incomingTransaction(const QModelIndex & parent, int start, int end)
{
    if(!walletModel || !clientModel)
        return;
    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
                    .data(Qt::EditRole).toULongLong();
    if(!clientModel->inInitialBlockDownload())
    {
        // On new transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date = ttm->index(start, TransactionTableModel::Date, parent)
                        .data().toString();
        QString type = ttm->index(start, TransactionTableModel::Type, parent)
                        .data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent)
                        .data().toString();
        QIcon icon = qvariant_cast<QIcon>(ttm->index(start,
                            TransactionTableModel::ToAddress, parent)
                        .data(Qt::DecorationRole));

        notificator->notify(Notificator::Information,
                            (amount)<0 ? tr("Sent transaction") :
                                         tr("Incoming transaction"),
                              tr("Date: %1\n"
                                 "Amount: %2\n"
                                 "Type: %3\n"
                                 "Address: %4\n")
                              .arg(date)
                              .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
                              .arg(type)
                              .arg(address), icon);
    }
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    centralWidget->setCurrentWidget(overviewPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    centralWidget->setCurrentWidget(transactionsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), transactionView, SLOT(exportClicked()));
}

void BitcoinGUI::gotoAddressBookPage()
{
    addressBookAction->setChecked(true);
    centralWidget->setCurrentWidget(addressBookPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), addressBookPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(receiveCoinsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), receiveCoinsPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoSendCoinsPage()
{
    sendCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(sendCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoMintingPage()
{
    mintingViewAction->setChecked(true);
    centralWidget->setCurrentWidget(mintingPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoBlockexplorerPage()
{
   blockexplorerAction->setChecked(true);
   centralWidget->setCurrentWidget(blockExplorer);

   exportAction->setEnabled(false);
   disconnect(exportAction, SIGNAL(triggered()), 0, 0);

}


void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void BitcoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        int nValidUrisFound = 0;
        QList<QUrl> uris = event->mimeData()->urls();
        foreach(const QUrl &uri, uris)
        {
            if (sendCoinsPage->handleURI(uri.toString()))
                nValidUrisFound++;
        }

        // if valid URIs were found
        if (nValidUrisFound)
            gotoSendCoinsPage();
        else
            notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid Peepcoin address or malformed URI parameters."));
    }

    event->acceptProposedAction();
}

void BitcoinGUI::handleURI(QString strURI)
{
    // URI has to be valid
    if (sendCoinsPage->handleURI(strURI))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
    }
    else
        notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid Peepcoin address or malformed URI parameters."));
}

void BitcoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}

void BitcoinGUI::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt:
                                     AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus(walletModel->getEncryptionStatus());
}

void BitcoinGUI::backupWallet()
{
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    QString filename = QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if(!filename.isEmpty()) {
        if(!walletModel->backupWallet(filename)) {
            QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."));
        }
    }
}

void BitcoinGUI::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void BitcoinGUI::unlockWallet()
{
    if(!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if(walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog::Mode mode = sender() == unlockWalletAction ?
              AskPassphraseDialog::UnlockStaking : AskPassphraseDialog::Unlock;
        AskPassphraseDialog dlg(mode, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void BitcoinGUI::lockWallet()
{
    if(!walletModel)
        return;

    walletModel->setWalletLocked(true);
}

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void BitcoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BitcoinGUI::updateWeight()
{
    if (!pwalletMain)
        return;

    TRY_LOCK(cs_main, lockMain);
    if (!lockMain)
        return;

    TRY_LOCK(pwalletMain->cs_wallet, lockWallet);
    if (!lockWallet)
        return;

    uint64_t nMinWeight = 0, nMaxWeight = 0;
    pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);
}

void BitcoinGUI::updateStakingIcon()
{
    updateWeight();

    if (nLastCoinStakeSearchInterval && nWeight)
    {
        uint64_t nNetworkWeight = GetPoSKernelPS();
        unsigned nEstimateTime = nTargetSpacing * nNetworkWeight / nWeight;

        QString text;
        if (nEstimateTime < 60)
        {
            text = tr("%n second(s)", "", nEstimateTime);
        }
        else if (nEstimateTime < 60*60)
        {
            text = tr("%n minute(s)", "", nEstimateTime/60);
        }
        else if (nEstimateTime < 24*60*60)
        {
            text = tr("%n hour(s)", "", nEstimateTime/(60*60));
        }
        else
        {
            text = tr("%n day(s)", "", nEstimateTime/(60*60*24));
        }

        labelStakingIcon->setPixmap(QIcon(":/icons/staking_on").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelStakingIcon->setToolTip(tr("Staking.<br>Your weight is %1<br>Network weight is %2<br>Expected time to earn reward is %3").arg(nWeight).arg(nNetworkWeight).arg(text));
    }
    else
    {
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_off").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        if (pwalletMain && pwalletMain->IsLocked())
            labelStakingIcon->setToolTip(tr("Not staking because wallet is locked"));
        else if (vNodes.empty())
            labelStakingIcon->setToolTip(tr("Not staking because wallet is offline"));
        else if (IsInitialBlockDownload())
            labelStakingIcon->setToolTip(tr("Not staking because wallet is syncing"));
        else if (!nWeight)
            labelStakingIcon->setToolTip(tr("Not staking because you don't have mature coins"));
        else
            labelStakingIcon->setToolTip(tr("Not staking"));
    }
}
