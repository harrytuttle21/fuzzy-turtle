#pragma once
#include "sequencer.h"

#include <QStringList>
#include <algorithm>
#include <QWidget>

class PatternView;

// Vue TRACKER, comme les anciens trackers (et la vue TRACKER du pilote) : les lignes defilent
// de haut en bas, une colonne par piste (= un son) avec 3 champs : NOTE, VELocite, LONGueur.
// Le curseur et la selection sont ceux de la vue Pattern : la Maschine, Copier / Coller,
// Shift + pads et la transposition marchent pareil dans les deux vues.
class TrackerView : public QWidget
{
    Q_OBJECT
public:
    enum Field { Note = 0, Velocity = 1, Length = 2 };

    TrackerView (const seq::Sequencer& sequencer, const session::State& state, PatternView& cursor, QWidget* parent = nullptr);

    int field() const { return fieldIndex; }
    void setField (int f) { fieldIndex = f; update(); }
    void moveCursor (int rows, int fields);           // fleches : lignes, et champs (puis pistes)
    void setPlayRow (int row) { playRow = row; update(); }
    void setPlaying (bool p) { playing = p; update(); }
    void setEditMode (bool on) { editOn = on; update(); }
    void setFollow (bool on) { follow = on; update(); }
    void setTyping (const QString& digits) { typed = digits; update(); }   // chiffres en cours de saisie
    void setTrackNames (const QStringList& n) { names = n; update(); }
    void setSingleTrack (bool on) { single = on; update(); }   // la piste du son choisi seule (Piano Roll)
    bool singleTrack() const { return single; }

    static QString noteText (int note);               // « C-4 », « C#4 »
    QSize minimumSizeHint() const override { return { 300, 150 }; }

signals:
    void cellMenu (int row, int track, const QPoint& globalPos);   // clic droit
    void cellDoubleClicked (int row, int track, int field);        // double-clic
    void trackClicked (int track, bool solo);                      // en-tete : mute (Shift = solo)
    void cellClicked (int track);                                  // la piste = le son choisi
    void valueWheel (int row, int track, int field, int steps);   // Option + molette sur un champ

protected:
    void paintEvent (QPaintEvent*) override;
    void mousePressEvent (QMouseEvent*) override;
    void mouseMoveEvent (QMouseEvent*) override;
    void mouseReleaseEvent (QMouseEvent*) override;
    void mouseDoubleClickEvent (QMouseEvent*) override;
    void contextMenuEvent (QContextMenuEvent*) override;
    void wheelEvent (QWheelEvent*) override;

private:
    QFont mono() const;
    int charW() const;
    int rowH() const;
    int trackW() const { return charW() * (single ? 19 : 13); }   // « C-4 100 01 » + marges (une piste : plus large)
    int colOf (int f) const { return single ? (f == 0 ? 1 : f == 1 ? 7 : 13) : (f == 0 ? 0 : f == 1 ? 4 : 8); }
    int numW() const { return charW() * 5; }
    int headerH() const { return rowH() + 6; }
    int visibleRows() const;
    int visibleTracks() const;
    int firstRow() const;                             // ligne en haut de la vue
    bool hit (const QPointF& p, int& row, int& track, int& f) const;

    const seq::Sequencer& sequencer;
    const session::State& state;
    PatternView& cursor;
    int fieldIndex = Note;
    int playRow = 0;
    bool playing = false, editOn = true, follow = true;
    int firstTrack = 0;                               // defilement horizontal
    QString typed;
    QStringList names;
    bool single = false;
    int soundTrack() const { return std::clamp (state.sound - 1, 0, seq::kTracks - 1); }                                // noms des 16 sons (en-tete)
    // Cadre de selection a la souris
    bool banding = false;
    int bandRow = -1, bandTrack = -1;
};
