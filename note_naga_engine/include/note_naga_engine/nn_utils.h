#pragma once

#include <note_naga_engine/note_naga_api.h>
#include <note_naga_engine/core/types.h>

/**
 * @brief Statická třída obsahující sadu nástrojů pro úpravu a manipulaci
 * s MIDI sekvencemi (NoteNagaMidiSeq).
 *
 * Všechny metody jsou statické a operují na referenci předaného objektu NoteNagaMidiSeq.
 * Modifikační funkce emitují signál metadataChanged pouze jednou na konci operace,
 * aby se zabránilo nadměrnému zatížení GUI.
 */
class NOTE_NAGA_ENGINE_API NN_Utils
{
public:
    /**
     * @brief Kvantizuje začátky not v sekvenci na definovanou mřížku.
     * @param seq Sekvence k úpravě.
     * @param grid_divisor Dělitel čtvrťové noty pro mřížku (např. 4 pro 16-tinové noty, 3 pro 8-nové trioly).
     */
    static void quantize(NoteNagaMidiSeq &seq, int grid_divisor);

    /**
     * @brief Přidá náhodné "lidské" odchylky do časování a dynamiky not.
     * @param seq Sekvence k úpravě.
     * @param time_strength Maximální odchylka v ticích (plus/mínus).
     * @param vel_strength Maximální odchylka ve velocitě (plus/mínus).
     */
    static void humanize(NoteNagaMidiSeq &seq, int time_strength, int vel_strength);

    /**
     * @brief Transponuje všechny noty v sekvenci o daný počet půltónů.
     * @param seq Sekvence k úpravě.
     * @param semitones Počet půltónů pro transpozici (může být kladný i záporný).
     */
    static void transpose(NoteNagaMidiSeq &seq, int semitones);

    /**
     * @brief Změní velocitu všech not.
     * @param seq Sekvence k úpravě.
     * @param value Hodnota pro změnu (buď absolutní, nebo procentuální).
     * @param relative Pokud je true, 'value' je procento (100 = beze změny). Pokud false, 'value' je nová absolutní hodnota.
     */
    static void changeVelocity(NoteNagaMidiSeq &seq, int value, bool relative = false);

    /**
     * @brief Změní délku (délku trvání) všech not.
     * @param seq Sekvence k úpravě.
     * @param value Hodnota pro změnu (buď absolutní v ticích, nebo procentuální).
     * @param relative Pokud je true, 'value' je procento (100 = beze změny). Pokud false, 'value' je nová absolutní délka.
     */
    static void changeDuration(NoteNagaMidiSeq &seq, int value, bool relative = false);

    /**
     * @brief Prodlouží noty tak, aby plynule navazovaly na další (legato).
     * @param seq Sekvence k úpravě.
     * @param strength_percent Síla efektu v procentech (100% znamená, že nota končí přesně tam, kde začíná další).
     */
    static void legato(NoteNagaMidiSeq &seq, int strength_percent = 100);

    /**
     * @brief Zkrátí délku not pro vytvoření staccato efektu.
     * @param seq Sekvence k úpravě.
     * @param strength_percent Procento původní délky, na které se noty zkrátí (např. 50% zkrátí noty na polovinu).
     */
    static void staccato(NoteNagaMidiSeq &seq, int strength_percent = 50);

    /**
     * @brief Invertuje (převrátí) výšku not kolem zadané osy.
     * @param seq Sekvence k úpravě.
     * @param axis_note MIDI číslo noty, která slouží jako osa inverze.
     */
    static void invert(NoteNagaMidiSeq &seq, int axis_note = 60); // Střední C jako výchozí

    /**
     * @brief Obrátí pořadí not v každé stopě (retrográdní).
     * @param seq Sekvence k úpravě.
     */
    static void retrograde(NoteNagaMidiSeq &seq);

    /**
     * @brief Odstraní překrývající se noty se stejnou výškou v každé stopě.
     * @param seq Sekvence k úpravě.
     */
    static void deleteOverlappingNotes(NoteNagaMidiSeq &seq);

    /**
     * @brief Změní měřítko časování (start a délka) všech not o zadaný faktor.
     * @param seq Sekvence k úpravě.
     * @param factor Faktor změny (např. 2.0 pro dvojnásobnou rychlost, 0.5 pro poloviční).
     */
    static void scaleTiming(NoteNagaMidiSeq &seq, double factor);

private:
    /**
     * @brief Privátní konstruktor, aby se zabránilo vytváření instancí této třídy.
     */
    NN_Utils() = default;
};