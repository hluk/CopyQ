Translations
============

Translations can be done either via
`Weblate <https://hosted.weblate.org/projects/copyq/>`__ (preferred) or
by using Qt utilities.

For explanation for some frequent words see :ref:`glossary`.

Translating Keyboard Accelerators
---------------------------------

Some texts contain single ``&`` character that is not visible in UI and is used
to mark the following character as keyboard accelerator (the character is
usually underlined in UI). This is used to quickly access labels, menu items
etc. with keyboard shortcut.

E.g. ``&File`` menu item can be accesses with ``Alt+F`` shortcut on most
systems.

If multiple UI elements have the same keyboard accelerator, associated shortcut
cycles through them (if pressed multiple times). It's better to avoid this by
defining **unique accelerator**, but that's not always easy.

If unsure, use the original one enclosed in parentheses, e.g. label
``For&mat:`` can be translated to simplified Chinese as ``格式(&M):``.

Writing Translatable Code
-------------------------

All GUI strings should be translatable. This is indicated in code with
``tr("Some GUI text", "Hints for translators")``.

Adding New Language
-------------------

To add new language for the application follow these steps.

1. Create new translation file with
   ``utils/lupdate.sh translations/copyq_<LANGUAGE>.ts``.
2. Add new language file to Git repository.
3. Translate with Weblate service or locally with
   ``linguist translations/copyq_<LANGUAGE>.ts``.
