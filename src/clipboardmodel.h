#ifndef CLIPBOARDMODEL_H
#define CLIPBOARDMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QRegExp>
#include <QList>
// Qt::escape
#include <QTextDocument>
#include <QDebug>
#include <QImage>

static const QRegExp re_spaces("  | |\t|\n+$");
#define ESCAPE(str) (\
    Qt::escape(str).replace\
        (re_spaces,"&nbsp;").replace\
        ('\n', "<br />"))

const QModelIndex empty_index;

class ClipboardItem : public QString
{
public:
    ClipboardItem(const QString &str) : QString(str),
        m_highlight(NULL), m_image(NULL), m_filtered(false) {}

    ~ClipboardItem()
    {
        removeHighlight();
        removeImage();
    }

    void setHighlight(const QString &str) const
    {
        if (m_highlight)
            *m_highlight = str;
        else
            m_highlight = new QString(str);
    }

    const QString &highlighted() const
    {
        if ( !m_highlight )
            setHighlight( ESCAPE(*(dynamic_cast<const QString*>(this))) );
        return *m_highlight;
    }

    void removeHighlight()
    {
        if (m_highlight) {
            delete m_highlight;
            m_highlight = NULL;
        }
    }

    void removeImage()
    {
        if ( m_image ) {
            delete m_image;
            m_image =  NULL;
        }
    }

    bool isFiltered() const { return m_filtered; }
    void setFiltered(bool filtered) {
        m_filtered = filtered;
        removeHighlight();
    }

    // image
    void setImage(const QImage &image) {
        removeHighlight();
        clear();
        append( QString("IMAGE") );
        if (!m_image)
            m_image = new QImage(image);
        else
            *m_image = image;
    }

    const QImage *image() const {
        return m_image;
    }

private:
    mutable QString *m_highlight;
    QImage *m_image;
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
    void clear() {
        m_clipboardList.clear();
    }

    void setMaxItems(int max) { m_max = max; }

    bool move(int pos, int newpos);

    // search
    const QRegExp *search() const { return &m_re; }
    void setSearch(const QRegExp *const re = NULL);
    void setSearch(int i);
    bool isFiltered(int i) const; // is item filtered out

    int getRowNumber(int row, bool cycle = false) const;

    bool isImage(int row) const {
        return row < rowCount() &&
                m_clipboardList[row].image() != NULL;
    }
    const QImage image(int row) const {
        if ( isImage(row) )
            return *(m_clipboardList[row].image());
        else
            return QImage();
    }

private:
    QList<ClipboardItem> m_clipboardList;
    QRegExp m_re;
    int m_max;
};

#endif // CLIPBOARDMODEL_H
