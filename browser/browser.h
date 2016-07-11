#ifndef BROWSER_H
#define BROWSER_H

#include <memory>
#include <QMainWindow>

#include "treemodel.hpp"

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

private slots:
    void on_actionOpen_triggered();
    void on_treeView_clicked(const QModelIndex &index);
    void onTreeSelectionChanged( const QModelIndex& index );
    void on_treeView_expanded(const QModelIndex &index);

private:
    void FillTree();
    void RecursiveExpand(const QModelIndex& index);
    void SetText( const char* txt );

    Ui::Browser *ui;
    std::unique_ptr<Archive> m_archive;
    std::unique_ptr<TreeModel> m_model;
};

#endif
