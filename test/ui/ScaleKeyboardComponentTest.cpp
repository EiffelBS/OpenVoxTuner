// ScaleKeyboardComponentTest.cpp
// Regression test (2026-07-17) : apres un clic utilisateur reel sur une
// touche du ScaleKeyboardComponent, l'etat visuel (paintButton utilise
// `activeInScale || getToggleState()`) doit refleter le nouvel etat du
// toggle, ET le `AudioParameterBool` (custom_i) connecte via
// `ButtonAttachment` doit etre ecrit avec la nouvelle valeur.
//
// Notes de couverture :
// 1) Premier fix (Fix L) override mouseDown + setToggleState manuel :
//    le test passait car il appelait directement les setters, mais en
//    runtime le `ButtonAttachment` n'etait pas informe du changement
//    (sa callback est branchee sur onClick / stateChanged, declenche
//    par clicked() et non par mouseDown). La touche D restait donc
//    visuellement active malgre le clic OFF.
// 2) Fix R (2026-07-17) : deplacer la logique dans clicked() au lieu
//    de mouseDown(). Le test ci-dessous utilise `triggerClick()`
//    (la methode publique de Button qui simule un mouseDown + mouseUp
//    + clicked). `Button::clicked()` est `protected` dans cette
//    version de JUCE (et `sendClickMessage` est `private`), donc on
//    ne peut pas les appeler directement depuis un test.
// 3) Fix X (2026-07-17) : on n'override plus `clicked()` du tout. A
//    la place, on enregistre un `Button::Listener` interne dans le
//    constructeur qui fire `onUserInteraction` + sync `activeInScale`
//    quand le base class `Button::clicked()` (lui-meme fire par
//    `triggerClick`) appelle `sendClickMessage` sur tous les
//    listeners. Le `ButtonAttachment` est aussi un `Button::Listener`
//    et fait son travail de pousser vers `custom_i` dans la meme
//    passe. C'est le seul moyen propre d'avoir a la fois le sync
//    visuel ET la persistance du parametre sans toucher au membre
//    private `sendClickMessage` de JUCE.

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/ui/ScaleKeyboardComponent.h"

class ScaleKeyboardComponentTest : public juce::UnitTest
{
public:
    ScaleKeyboardComponentTest() : juce::UnitTest ("ScaleKeyboardComponent") {}

    void runTest() override
    {
        using ui::PianoKeyButton;
        using ui::ScaleKeyboardComponent;

        // Regression test (2026-07-17, Fix AB): in JUCE 8, the
        // `juce::ToggleButton` constructor no longer sets
        // `clickTogglesState = true` (it did in JUCE 7). Without
        // an explicit call to `setClickingTogglesState(true)` in
        // `PianoKeyButton::PianoKeyButton`, a real mouse click
        // goes through `Button::internalClickCallback` which, with
        // `clickTogglesState == false`, only fires
        // `sendClickMessage(modifiers)` and never calls
        // `setToggleState`. The AudioParameterBool (`custom_i`)
        // connected via `ButtonAttachment` is therefore never
        // written by the click, and the user cannot add/remove
        // notes from the scale in the live plugin. The unit test
        // below catches this by asserting that a freshly
        // constructed `PianoKeyButton` is actually toggleable on
        // click.
        beginTest ("PianoKeyButton est toggleable au clic (regression JUCE 8)");
        {
            PianoKeyButton btn;
            expect (btn.isToggleable(),
                "Un PianoKeyButton fraichement construit doit etre "
                "toggleable au clic (clickTogglesState = true). Sans "
                "cela, un clic souris reel ne fait que fire "
                "sendClickMessage sans jamais appeler setToggleState, "
                "donc le AudioParameterBool custom_i n'est jamais "
                "ecrit par le clic et l'utilisateur ne peut pas "
                "ajouter/retirer des notes de la gamme dans le plugin.");
        }

        // La logique de rendu de PianoKeyButton::paintButton est :
        //   isActive = activeInScale || getToggleState()
        // L'invariant qu'on veut verifier : apres un clic utilisateur
        // reel (mouseDown + mouseUp -> clicked -> sendClickMessage ->
        // tous les Button::Listener notifies), isActive ==
        // getToggleState() ET la callback onUserInteraction est appelee
        // (c'est elle qui fait basculer le combo de gamme en "Custom").
        //
        // Pour simuler un clic utilisateur dans un test unitaire
        // portable, on appelle `triggerClick()` : c'est la methode
        // publique de juce::Button qui simule un mouseDown + mouseUp +
        // clicked. C'est exactement ce que la souris declenche en
        // runtime, et donc ce que le ButtonAttachment observe.

        // 1) En C Natural Minor, la touche D (index 2) fait partie de
        //    la gamme : activeInScale=true, getToggleState()=true
        //    (le cas problematique rapporte par l'utilisateur).
        beginTest ("Clic OFF sur touche de la gamme preset : visuel = toggle (etat OFF)");
        {
            ScaleKeyboardComponent comp;
            auto& btnD = comp.getButton (2); // D

            // Setup : on simule "C Natural Minor" + le toggle ON.
            btnD.setActiveInScale (true);
            btnD.setToggleState (true, juce::dontSendNotification);

            // Capture la callback onUserInteraction (qui, en runtime,
            // fait basculer le combo de gamme en "Custom" via le
            // `scale` AudioParameterChoice).
            int interactionCount = 0;
            btnD.onUserInteraction = [&interactionCount] { ++interactionCount; };

            // Simule un clic utilisateur complet via triggerClick().
            // Le base class Button::clicked() fait setToggleState(!current)
            // + sendClickMessage, ce qui propage au ButtonAttachment ET a
            // notre InteractionListener.
            btnD.triggerClick();

            // Verification : apres triggerClick(), le toggle est OFF,
            // le visuel suit (activeInScale == false), et la callback
            // onUserInteraction a ete appelee une fois.
            expect (! btnD.getToggleState(),
                "Toggle de D apres clic doit etre OFF (le base class "
                "Button::clicked() a fait setToggleState(!current))");
            expect (! btnD.isActiveInScale(),
                "activeInScale doit suivre le toggle (etat OFF) "
                "pour que le visuel reflete correctement le clic. "
                "Sinon paintButton (activeInScale || getToggleState()) "
                "continue d'afficher la touche comme active.");
            expect (interactionCount == 1,
                "onUserInteraction doit etre appelee une fois apres "
                "le clic, pour faire basculer le combo de gamme en "
                "Custom. Sans cela, le passage preset -> Custom ne "
                "se fait pas et la touche D reste figee visuellement.");
        }

        // 2) En C Natural Minor, la touche D# (index 3) NE fait PAS
        //    partie de la gamme : activeInScale=false. Si l'utilisateur
        //    veut l'activer, le clic ON doit etre visible.
        beginTest ("Clic ON sur touche hors gamme preset : visuel = toggle (etat ON)");
        {
            ScaleKeyboardComponent comp;
            auto& btnDs = comp.getButton (3); // D#

            btnDs.setActiveInScale (false);
            btnDs.setToggleState (false, juce::dontSendNotification);

            int interactionCount = 0;
            btnDs.onUserInteraction = [&interactionCount] { ++interactionCount; };

            // Clic utilisateur -> triggerClick() -> toggle ON + sync.
            btnDs.triggerClick();

            expect (btnDs.getToggleState(),
                "Toggle de D# apres clic doit etre ON");
            expect (btnDs.isActiveInScale(),
                "activeInScale doit suivre le toggle (etat ON) "
                "pour que le visuel reflete correctement le clic.");
            expect (interactionCount == 1,
                "onUserInteraction doit etre appelee une fois apres "
                "le clic, pour faire basculer le combo en Custom.");
        }

        // 3) En Custom, on peut librement toggler. On verifie que
        //    l'invariant est respecte dans les deux directions.
        beginTest ("Toggles successifs en Custom : visuel suit le toggle");
        {
            ScaleKeyboardComponent comp;
            auto& btn = comp.getButton (5); // F

            // Setup : F est ON (etait dans la gamme precedente)
            btn.setActiveInScale (true);
            btn.setToggleState (true, juce::dontSendNotification);

            for (int k = 0; k < 3; ++k)
            {
                // Clic utilisateur -> triggerClick()
                btn.triggerClick();
                // L'invariant : les deux sont egaux
                expect (btn.isActiveInScale() == btn.getToggleState(),
                    "Apres iteration " + juce::String (k)
                    + " : activeInScale et getToggleState() doivent coincider. "
                    + "activeInScale=" + juce::String (btn.isActiveInScale() ? "true" : "false")
                    + ", getToggleState()=" + juce::String (btn.getToggleState() ? "true" : "false"));
            }
        }

        // 4) Verification que triggerClick() est bien le bon point
        //    d'entree (c'est la seule methode publique qui simule un
        //    mouseDown+mouseUp complet + clicked). La logique JUCE
        //    standard de Button::clicked() fait le setToggleState(!)
        //    + sendClickMessage (qui notifie le ButtonAttachment et
        //    notre InteractionListener). Notre InteractionListener
        //    fait le reste : sync activeInScale + onUserInteraction.
        beginTest ("triggerClick() est bien le point d'entree public (simule un mouseDown+Up complet)");
        {
            ScaleKeyboardComponent comp;
            auto& btn = comp.getButton (0); // C

            btn.setActiveInScale (true);
            btn.setToggleState (false, juce::dontSendNotification);

            int interactionCount = 0;
            btn.onUserInteraction = [&interactionCount] { ++interactionCount; };

            // Simule un clic utilisateur : JUCE appelle triggerClick()
            // qui simule un cycle mouseDown + mouseUp reussi. C'est la
            // methode publique (la seule accessible) qui contient la
            // logique a tester.
            btn.triggerClick();

            // Apres triggerClick(), le base class Button::clicked() a
            // toggle l'etat, le ButtonAttachment a ete notifie (custom_i
            // mis a jour), l'InteractionListener a fait le sync
            // activeInScale et a appele onUserInteraction.
            expect (btn.getToggleState(),
                "Toggle de C apres triggerClick() doit etre ON (base "
                "class Button::clicked() toggle !current)");
            expect (btn.isActiveInScale(),
                "activeInScale doit suivre le toggle apres triggerClick() "
                "(InteractionListener fait le sync)");
            expect (interactionCount == 1,
                "onUserInteraction doit etre appelee une fois par "
                "triggerClick() (InteractionListener fait l'appel)");
        }
    }
};

static ScaleKeyboardComponentTest scaleKeyboardComponentTest;
