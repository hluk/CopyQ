Translations
============

Translations can be done either via
`Weblate <https://hosted.weblate.org/projects/copyq/>`__ (preferred) or
by using Qt utilities.

All GUI strings should be translatable. This is indicated in code with
``tr("Some GUI text", "Hints for translators")``.

Adding New Language
-------------------

To add new language for the application follow these steps.

1. Edit ``copyq.pro`` and add file name for new language
   (``translations/copyq_<LANGUAGE>.ts``) to ``TRANSLATIONS`` variable.
2. Create new language file with ``lupdate copyq.pro``.
3. Add new language file to Git repository.
4. Translate with Weblate service or locally with
   ``linguist translations/copyq_<LANGUAGE>.ts``.
