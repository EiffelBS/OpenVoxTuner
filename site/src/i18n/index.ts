import en from './en.json';
import fr from './fr.json';
import de from './de.json';
import es from './es.json';
import ja from './ja.json';
import zh from './zh.json';

export const locales = ['en', 'fr', 'de', 'es', 'ja', 'zh'] as const;
export type Locale = typeof locales[number];

export const localeNames: Record<Locale, string> = {
  en: 'English',
  fr: 'Français',
  de: 'Deutsch',
  es: 'Español',
  ja: '日本語',
  zh: '中文',
};

const translations = { en, fr, de, es, ja, zh };

export function getTranslations(locale: Locale) {
  return translations[locale] || translations.en;
}
