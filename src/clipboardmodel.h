#ifndef CLIPBOARDMODEL_H
#define CLIPBOARDMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QRegExp>
#include <QList>
// Qt::escape
#include <QTextDocument>

#define ESCAPE(str) (Qt::escape(str).replace('\n', "<br />"))

class ClipboardItem : public QString
{
public:
    ClipboardItem(const QString &str) : QString(str),
        m_highlight(NULL), m_filtered(false) {}
    ~ClipboardItem()
    {
        removeHighlight();
    }

    void highlight(const QString &str) const
    {
        if (m_highlight)
            *m_highlight = str;
        else
            m_highlight = new QString(str);
    }

    QString highlighted() const
    {
        if (m_highlight)
            return *m_highlight;
        else
            highlight( ESCAPE(*(dynamic_cast<const QString*>(this))) );

        return *m_highlight;
    }

    void removeHighlight()
    {
        if (m_highlight) {
            delete m_highlight;
            m_highlight = NULL;
        }
    }

    bool isFiltered() const { return m_filtered; }
    void setFiltered(bool filtered) { m_filtered = filtered; }

private:
    mutable QString *m_highlight;
    bool m_filtered;
};

class ClipboardModel : public QAbstractListModel
{
public:
    ClipboardModel(const QStringList &items = QStringList());

    // need to be implemented
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    
    // editting
    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool setData(const QModelIndex &index, const QVariant &value,
                  int role = Qt::EditRole);
    bool insertRows(int position, int rows, const QModelIndex &index = QModelIndex());
    bool removeRows(int position, int rows, const QModelIndex &index = QModelIndex());

    void setMaxItems(int max) { m_max = max; }

    bool move(int pos, int newpos);

    // search
    const QRegExp *search() const { return &m_re; }
    void setSearch(const QRegExp *const re = NULL);
    bool isFiltered(int i) const; // is item filtered out

private:
    QList<ClipboardItem> m_clipboardList;
    QRegExp m_re;
    int m_max;
};

#endif // CLIPBOARDMODEL_H
