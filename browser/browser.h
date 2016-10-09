#ifndef BROWSER_H
#define BROWSER_H

#include <memory>
#include <stdint.h>
#include <vector>
#include <QMainWindow>

#include "treemodel.hpp"
#include "TextBuf.hpp"

class Archive;

namespace Ui
{
    class Browser;
}

class Browser : public QMainWindow
{
    Q_OBJECT

public:
    explicit Browser( QWidget *parent = 0 );
    ~Browser();

    void GoToDate( uint32_t date );

private slots:
    void on_actionOpen_triggered();
    void on_actionRaw_message_triggered(bool checked);
    void on_actionROT13_triggered(bool checked);
    void on_treeView_clicked(const QModelIndex &index);
    void onTreeSelectionChanged( const QModelIndex& index );
    void on_treeView_expanded(const QModelIndex &index);
    void on_lineEdit_returnPressed();
    void on_actionGo_to_message_triggered();
    void on_actionAbout_triggered();
    void on_actionGroup_Charter_triggered();

    void on_actionGo_to_date_triggered();

private:
    void FillTree();
    void RecursiveExpand(const QModelIndex& index);
    void RecursiveSetIndex(const QModelIndex& index);
    void SetText( const char* txt );
    void ShowMessage( const char* msg );
    void SwitchToMessage( uint32_t idx );
    void ClearSearch();

    Ui::Browser *ui;
    std::unique_ptr<Archive> m_archive;
    std::unique_ptr<TreeModel> m_model;
    TextBuf m_buf;
    int32_t m_index;
    bool m_rawMessage;
    bool m_rot13;
    std::vector<QWidget*> m_searchItems;
};

#endif
