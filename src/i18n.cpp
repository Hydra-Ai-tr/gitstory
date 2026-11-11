#include "i18n.hpp"

#include <cstdio>

#include "util.hpp"

namespace gitstory {

namespace {

Strings english() {
  Strings s{};
  s.commit = {"commit", "commits", "commits"};
  s.author = {"author", "authors", "authors"};
  s.day = {"day", "days", "days"};
  s.line = {"line", "lines", "lines"};
  s.file = {"file", "files", "files"};
  s.merge = {"merge", "merges", "merges"};
  s.character = {"character", "characters", "characters"};
  s.time = {"time", "times", "times"};
  s.revert = {"revert", "reverts", "reverts"};
  s.message = {"commit message", "commit messages", "commit messages"};
  s.person = {"person", "people", "people"};
  s.month = {"month", "months", "months"};
  s.year = {"year", "years", "years"};

  s.no_commits = "No commits found.";
  s.unknown_author = "(unknown)";
  s.no_message = "(no message)";

  s.section_activity = "Activity";
  s.section_rhythm = "Rhythm";
  s.section_highlights = "Highlights";
  s.section_authors = "Authors";
  s.section_languages = "Languages";
  s.section_files = "Hot files";
  s.section_words = "Words";
  s.section_messages = "Messages";
  s.section_roast = "Roast";

  s.story_one_day = "{0} came together in a single day, {1}, in {2} by {3}.";
  s.story_span = "{0} has been in the works for {1} ({2} to {3}), with {4} by {5}.";
  s.who_one = "{0}";
  s.who_two = "{0} and {1}";
  s.who_many = "{0}";
  s.story_active = "Something landed on {0} of those {1} days.";
  s.story_rhythm = "{0} is the busiest day of the week and {1} the busiest hour";
  s.story_night = "; {0} of commits land after midnight";
  s.story_weekend = "; {0} of commits land on weekends";
  s.story_grew = "The code grew by {0}";
  s.story_shrank = "The code shrank by {0}";
  s.story_top_file = ", and {0} changed more often than any other file";
  s.story_lang_one = "It is mostly {0}.";
  s.story_lang_two = "It is mostly {0} with a good helping of {1}.";
  s.story_lead = "{0} leads with {1} of the commits.";

  s.tile_active_days = "active days";
  s.tile_lines = "lines changed";

  s.activity_one_day = "Every commit happened on {0}. See the rhythm below for how the day went.";
  s.activity_summary = "{0} on {1} of {2} days";
  s.busiest = "busiest";
  s.less = "less";
  s.more = "more";
  s.each_column = "each column = {0}, square-root scale";

  s.phase_night = "night";
  s.phase_morning = "morning";
  s.phase_afternoon = "afternoon";
  s.phase_evening = "evening";
  s.busiest_day = "busiest day";
  s.busiest_hour = "busiest hour";
  s.weekends = "weekends";
  s.friday_late = "Friday after 4pm";

  s.hl_busiest_day = "Busiest day";
  s.hl_busiest_week = "Busiest week";
  s.hl_busiest_month = "Busiest month";
  s.hl_streak = "Longest streak";
  s.hl_break = "Longest break";
  s.hl_biggest = "Biggest commit";
  s.hl_night = "Night owl";
  s.week_of = "week of {0}";
  s.night_between = "{0} between 00:00 and 05:00 ({1})";

  s.more_authors = "... and {0} more";

  s.msg_average = "Average length";
  s.msg_conventional = "Conventional";
  s.msg_lazy = "Low-effort";
  s.msg_reverts = "Reverts";
  s.verdict_terse = "terse";
  s.verdict_to_the_point = "to the point";
  s.verdict_detailed = "detailed";
  s.of_commits = "{0} of commits";

  s.roast_night = "{0} of commits landed between midnight and 5am. Sleep is also a feature.";
  s.roast_healthy = "Not a single commit between midnight and 5am. Suspiciously healthy.";
  s.roast_lazy = "{0} say nothing at all{1}. Future you will be thrilled.";
  s.roast_lazy_top = " (\"{0}\" alone appears {1})";
  s.roast_short = "The average commit message is {1} long. Haiku have more detail.";
  s.roast_silent = "Went silent for {0} starting {1}. The repo waited patiently.";
  s.roast_big = "One commit changed {0}. \"Small refactor\", surely.";
  s.roast_weekend = "{0} of commits happened on a weekend. Hobbies are overrated anyway.";
  s.roast_friday = "{0} went in on a Friday after 4pm. Bold.";
  s.roast_hot_file = "{0} was touched in {1} of all commits. It has seen things.";
  s.roast_solo = "One author, {0}. A one-person band with a bus factor of exactly 1.";
  s.roast_dominant = "{0} wrote {1} of the commits. Everyone else is here for moral support.";
  s.roast_reverts = "{0}. Two steps forward, one `git revert` back.";
  s.roast_streak = "A {0}-day commit streak. Someone please check on this person.";
  s.roast_conventional =
      "Over 90% Conventional Commits. The linter is proud; the rest of us are a little scared.";
  s.roast_nothing =
      "Nothing to roast. Either this repo is flawless or it is too young to have made mistakes "
      "yet.";

  s.svg_when = "When commits happen";
  s.svg_night = "Night commits (00-05)";
  s.svg_weekend = "Weekend commits";
  s.svg_peak_hour = "Peak hour";
  s.svg_activity = "Activity over time";
  s.svg_words = "Most used words:";
  return s;
}

// Sentences are built so that no suffix has to attach to a number or a name:
// Turkish vowel harmony would otherwise need to know how "%19" is read aloud.
Strings turkish() {
  Strings s{};
  s.commit = {"commit", "commit", "commit"};
  s.author = {"yazar", "yazar", "yazar"};
  s.day = {"gün", "gün", "gün"};
  s.line = {"satır", "satır", "satır"};
  s.file = {"dosya", "dosya", "dosya"};
  s.merge = {"birleştirme", "birleştirme", "birleştirme"};
  s.character = {"karakter", "karakter", "karakter"};
  s.time = {"kez", "kez", "kez"};
  s.revert = {"geri alma", "geri alma", "geri alma"};
  s.message = {"commit mesajı", "commit mesajı", "commit mesajı"};
  s.person = {"kişi", "kişi", "kişi"};
  s.month = {"ay", "ay", "ay"};
  s.year = {"yıl", "yıl", "yıl"};

  s.no_commits = "Hiç commit bulunamadı.";
  s.unknown_author = "(bilinmiyor)";
  s.no_message = "(mesaj yok)";

  s.section_activity = "Aktivite";
  s.section_rhythm = "Ritim";
  s.section_highlights = "Öne çıkanlar";
  s.section_authors = "Yazarlar";
  s.section_languages = "Diller";
  s.section_files = "En çok değişen dosyalar";
  s.section_words = "Kelimeler";
  s.section_messages = "Commit mesajları";
  s.section_roast = "Taşlama";

  s.story_one_day = "{0} tek bir günde, {1} tarihinde hayat buldu: {3} toplam {2} attı.";
  s.story_span = "{0}, {2} ile {3} arasında {1} boyunca geliştirildi: {5} toplam {4} attı.";
  s.who_one = "{0}";
  s.who_two = "{0} ve {1}";
  s.who_many = "{0}";
  s.story_active = "Bu {1} günün {0} günü aktif geçti.";
  s.story_rhythm = "Haftanın en yoğun günü {0}, en yoğun saati {1}";
  s.story_night = "; commit'lerin {0} kadarı gece yarısından sonra geliyor";
  s.story_weekend = "; commit'lerin {0} kadarı hafta sonu geliyor";
  s.story_grew = "Kod {0} büyüdü";
  s.story_shrank = "Kod {0} küçüldü";
  s.story_top_file = ", en sık değişen dosya ise {0}";
  s.story_lang_one = "Proje ağırlıklı olarak {0} ile yazılmış.";
  s.story_lang_two = "Proje ağırlıklı olarak {0} ile yazılmış, epeyce de {1} var.";
  s.story_lead = "Commit'lerin {1} kadarı {0} imzalı.";

  s.tile_active_days = "aktif gün";
  s.tile_lines = "değişen satır";

  s.activity_one_day =
      "Tüm commit'ler {0} tarihinde atıldı. Günün nasıl geçtiğini aşağıdaki ritimde görebilirsin.";
  s.activity_summary = "{2} günün {1} gününde {0}";
  s.busiest = "en yoğun";
  s.less = "az";
  s.more = "çok";
  s.each_column = "her sütun = {0}, karekök ölçek";

  s.phase_night = "gece";
  s.phase_morning = "sabah";
  s.phase_afternoon = "öğleden sonra";
  s.phase_evening = "akşam";
  s.busiest_day = "en yoğun gün";
  s.busiest_hour = "en yoğun saat";
  s.weekends = "hafta sonu";
  s.friday_late = "Cuma 16:00 sonrası";

  s.hl_busiest_day = "En yoğun gün";
  s.hl_busiest_week = "En yoğun hafta";
  s.hl_busiest_month = "En yoğun ay";
  s.hl_streak = "En uzun seri";
  s.hl_break = "En uzun ara";
  s.hl_biggest = "En büyük commit";
  s.hl_night = "Gece kuşu";
  s.week_of = "{0} haftası";
  s.night_between = "{0}, 00:00 ile 05:00 arası ({1})";

  s.more_authors = "... ve {0} yazar daha";

  s.msg_average = "Ort. uzunluk";
  s.msg_conventional = "Conventional";
  s.msg_lazy = "Baştan savma";
  s.msg_reverts = "Geri almalar";
  s.verdict_terse = "çok kısa";
  s.verdict_to_the_point = "yerinde";
  s.verdict_detailed = "ayrıntılı";
  s.of_commits = "commit'lerin {0} kadarı";

  s.roast_night =
      "Commit'lerin {0} kadarı gece yarısı ile sabah 5 arasında atılmış. Uyku da bir özelliktir.";
  s.roast_healthy =
      "Gece yarısı ile sabah 5 arasında tek bir commit yok. Şüphe uyandıracak kadar sağlıklı.";
  s.roast_lazy = "{0} hiçbir şey anlatmıyor{1}. Gelecekteki sen buna çok sevinecek.";
  s.roast_lazy_top = " (yalnızca \"{0}\" {1} geçiyor)";
  s.roast_short = "Ortalama commit mesajı {1}. Haikular bile daha ayrıntılı.";
  s.roast_silent = "{1} tarihinden itibaren {0} boyunca ses çıkmadı. Repo sabırla bekledi.";
  s.roast_big = "Tek bir commit {0} değiştirdi. \"Ufak bir refactor\", eminim.";
  s.roast_weekend = "Commit'lerin {0} kadarı hafta sonu atılmış. Hobiler zaten abartılıyor.";
  s.roast_friday = "{0} cuma 16:00'dan sonra atılmış. Cesurca.";
  s.roast_hot_file = "{0} commit'lerin {1} kadarında değişmiş. Neler gördü neler.";
  s.roast_solo = "Tek yazar, {0}. Otobüs faktörü tam olarak 1 olan tek kişilik bir orkestra.";
  s.roast_dominant = "Commit'lerin {1} kadarı {0} imzalı. Diğerleri manevi destek için burada.";
  s.roast_reverts = "{0}. İki adım ileri, bir `git revert` geri.";
  s.roast_streak = "Aralıksız {1} commit serisi. Biri şu kişiye bir baksın.";
  s.roast_conventional =
      "Commit'lerin %90'ından fazlası Conventional Commits. Linter gurur duyuyor; geri kalanımız "
      "biraz korkuyor.";
  s.roast_nothing =
      "Taşlanacak bir şey yok. Ya bu repo kusursuz ya da henüz hata yapacak kadar büyümedi.";

  s.svg_when = "Commit'ler ne zaman geliyor";
  s.svg_night = "Gece commit'leri (00-05)";
  s.svg_weekend = "Hafta sonu commit'leri";
  s.svg_peak_hour = "En yoğun saat";
  s.svg_activity = "Zaman içinde aktivite";
  s.svg_words = "En sık kelimeler:";
  return s;
}

// Nouns after numbers follow Russian plural rules; `line` is in the accusative
// because every sentence uses it as an object ("вырос на 14 строк").
Strings russian() {
  Strings s{};
  s.commit = {"коммит", "коммита", "коммитов"};
  s.author = {"автор", "автора", "авторов"};
  s.day = {"день", "дня", "дней"};
  s.line = {"строку", "строки", "строк"};
  s.file = {"файл", "файла", "файлов"};
  s.merge = {"слияние", "слияния", "слияний"};
  s.character = {"символ", "символа", "символов"};
  s.time = {"раз", "раза", "раз"};
  s.revert = {"откат", "отката", "откатов"};
  s.message = {"сообщение", "сообщения", "сообщений"};
  s.person = {"человек", "человека", "человек"};
  s.month = {"месяц", "месяца", "месяцев"};
  s.year = {"год", "года", "лет"};

  s.no_commits = "Коммиты не найдены.";
  s.unknown_author = "(неизвестно)";
  s.no_message = "(без сообщения)";

  s.section_activity = "Активность";
  s.section_rhythm = "Ритм";
  s.section_highlights = "Главное";
  s.section_authors = "Авторы";
  s.section_languages = "Языки";
  s.section_files = "Горячие файлы";
  s.section_words = "Слова";
  s.section_messages = "Сообщения";
  s.section_roast = "Прожарка";

  s.story_one_day = "{0} появился за один день, {1}: {2}, {3}.";
  s.story_span = "{0} в разработке уже {1} (с {2} по {3}): {4}, {5}.";
  s.who_one = "автор — {0}";
  s.who_two = "авторы — {0} и {1}";
  s.who_many = "участников: {1}";
  s.story_active = "Активных дней — {0} из {1}.";
  s.story_rhythm = "Самый активный день недели — {0}, самый активный час — {1}";
  s.story_night = "; {0} коммитов сделаны после полуночи";
  s.story_weekend = "; {0} коммитов приходится на выходные";
  s.story_grew = "Код вырос на {0}";
  s.story_shrank = "Код сократился на {0}";
  s.story_top_file = ", а чаще всего менялся файл {0}";
  s.story_lang_one = "В основном проект написан на {0}.";
  s.story_lang_two = "В основном проект написан на {0}, заметная доля — {1}.";
  s.story_lead = "Больше всех коммитов у {0} — {1}.";

  s.tile_active_days = "активных дней";
  s.tile_lines = "строк изменено";

  s.activity_one_day = "Все коммиты сделаны {0}. Как прошёл день — смотрите ритм ниже.";
  s.activity_summary = "{0}, активных дней: {1} из {2}";
  s.busiest = "пик";
  s.less = "меньше";
  s.more = "больше";
  s.each_column = "столбец = {0}, шкала — квадратный корень";

  s.phase_night = "ночь";
  s.phase_morning = "утро";
  s.phase_afternoon = "день";
  s.phase_evening = "вечер";
  s.busiest_day = "пиковый день";
  s.busiest_hour = "пиковый час";
  s.weekends = "выходные";
  s.friday_late = "пятница после 16:00";

  s.hl_busiest_day = "Лучший день";
  s.hl_busiest_week = "Лучшая неделя";
  s.hl_busiest_month = "Лучший месяц";
  s.hl_streak = "Макс. серия";
  s.hl_break = "Макс. перерыв";
  s.hl_biggest = "Самый крупный";
  s.hl_night = "Ночные коммиты";
  s.week_of = "неделя с {0}";
  s.night_between = "{0} с 00:00 до 05:00 ({1})";

  s.more_authors = "... и ещё {0}";

  s.msg_average = "Средняя длина";
  s.msg_conventional = "Conventional";
  s.msg_lazy = "Ни о чём";
  s.msg_reverts = "Откаты";
  s.verdict_terse = "лаконично";
  s.verdict_to_the_point = "по делу";
  s.verdict_detailed = "подробно";
  s.of_commits = "{0} коммитов";

  s.roast_night = "{0} коммитов сделаны между полуночью и пятью утра. Сон — тоже фича.";
  s.roast_healthy = "Ни одного коммита между полуночью и пятью утра. Подозрительно здорово.";
  s.roast_lazy = "{0} ни о чём не говорят{1}. Вы из будущего будете в восторге.";
  s.roast_lazy_top = " (одно только \"{0}\" встречается {1})";
  s.roast_short = "Средняя длина сообщения — {1}. Даже в хайку больше подробностей.";
  s.roast_silent = "Тишина длиной {0}, начиная с {1}. Репозиторий терпеливо ждал.";
  s.roast_big = "Один коммит изменил {0}. «Небольшой рефакторинг», конечно.";
  s.roast_weekend = "{0} коммитов сделаны в выходные. Хобби всё равно переоценены.";
  s.roast_friday = "{0} — в пятницу после 16:00. Смело.";
  s.roast_hot_file = "Файл {0} менялся в {1} всех коммитов. Он многое повидал.";
  s.roast_solo = "Один автор, {0}. Оркестр из одного человека с bus factor ровно 1.";
  s.roast_dominant = "На {0} приходится {1} коммитов. Остальные здесь для моральной поддержки.";
  s.roast_reverts = "{0}. Два шага вперёд, один `git revert` назад.";
  s.roast_streak = "Серия коммитов длиной {1}. Кто-нибудь, проверьте, как он там.";
  s.roast_conventional =
      "Больше 90% — Conventional Commits. Линтер гордится, остальные немного побаиваются.";
  s.roast_nothing =
      "Прожаривать нечего. Либо репозиторий безупречен, либо он слишком молод, чтобы ошибаться.";

  s.svg_when = "Когда появляются коммиты";
  s.svg_night = "Ночные коммиты (00-05)";
  s.svg_weekend = "Коммиты в выходные";
  s.svg_peak_hour = "Пиковый час";
  s.svg_activity = "Активность во времени";
  s.svg_words = "Частые слова:";
  return s;
}

const Strings& strings_for(Lang lang) {
  static const Strings en = english();
  static const Strings tr = turkish();
  static const Strings ru = russian();
  switch (lang) {
    case Lang::Tr: return tr;
    case Lang::Ru: return ru;
    case Lang::En: break;
  }
  return en;
}

constexpr const char* kWeekdayShort[3][7] = {
    {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"},
    {"Pzt", "Sal", "Çar", "Per", "Cum", "Cmt", "Paz"},
    {"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"},
};
constexpr const char* kWeekday[3][7] = {
    {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"},
    {"Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi", "Pazar"},
    {"Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота", "Воскресенье"},
};
constexpr const char* kWeekdayInSentence[3][7] = {
    {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"},
    {"pazartesi", "salı", "çarşamba", "perşembe", "cuma", "cumartesi", "pazar"},
    {"понедельник", "вторник", "среда", "четверг", "пятница", "суббота", "воскресенье"},
};
constexpr const char* kMonthShort[3][12] = {
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"},
    {"Oca", "Şub", "Mar", "Nis", "May", "Haz", "Tem", "Ağu", "Eyl", "Eki", "Kas", "Ara"},
    {"янв", "фев", "мар", "апр", "май", "июн", "июл", "авг", "сен", "окт", "ноя", "дек"},
};
// Russian dates put the month in the genitive: "3 мая", not "3 май".
constexpr const char* kRussianMonthOfDate[12] = {"янв", "фев", "мар", "апр", "мая", "июн",
                                                 "июл", "авг", "сен", "окт", "ноя", "дек"};

std::size_t index_of(Lang lang) { return static_cast<std::size_t>(lang); }

std::string with_decimal_comma(std::string text) {
  for (char& c : text) {
    if (c == '.') c = ',';
  }
  return text;
}

}  // namespace

std::optional<Lang> parse_lang(std::string_view text) {
  const std::string lower = lower_utf8(trim(text));
  const auto starts = [&](std::string_view prefix) {
    return lower.size() >= prefix.size() && lower.compare(0, prefix.size(), prefix) == 0 &&
           (lower.size() == prefix.size() || lower[prefix.size()] == '_' ||
            lower[prefix.size()] == '-' || lower[prefix.size()] == '.');
  };
  if (starts("en") || lower == "english" || lower == "eng") return Lang::En;
  if (starts("tr") || lower == "turkish" || lower == "türkçe" || lower == "turkce" ||
      lower == "tur") {
    return Lang::Tr;
  }
  if (starts("ru") || lower == "russian" || lower == "русский" || lower == "rus") return Lang::Ru;
  return std::nullopt;
}

const char* lang_code(Lang lang) {
  switch (lang) {
    case Lang::Tr: return "tr";
    case Lang::Ru: return "ru";
    case Lang::En: break;
  }
  return "en";
}

Locale::Locale(Lang lang) : lang_(lang), text_(&strings_for(lang)) {}

std::string Locale::number(std::int64_t n) const {
  std::string out = format_number(n);
  if (lang_ == Lang::En) return out;
  // U+00A0 keeps "1 213" together when a sentence wraps.
  const std::string separator = lang_ == Lang::Tr ? "." : "\u00A0";
  std::string localized;
  for (const char c : out) {
    if (c == ',') {
      localized += separator;
    } else {
      localized += c;
    }
  }
  return localized;
}

std::string Locale::percent(double ratio) const {
  const double pct = ratio * 100.0;
  if (pct > 0.0 && pct < 1.0) return lang_ == Lang::Tr ? "<%1" : "<1%";
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.0f", pct);
  return lang_ == Lang::Tr ? std::string("%") + buf : std::string(buf) + "%";
}

std::string Locale::percent1(double ratio) const {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.1f", ratio * 100.0);
  std::string value = buf;
  for (char& c : value) {
    if (c == ',') c = '.';  // whatever the C locale says
  }
  switch (lang_) {
    case Lang::Tr: return "%" + with_decimal_comma(value);
    case Lang::Ru: return with_decimal_comma(value) + "%";
    case Lang::En: break;
  }
  return value + "%";
}

std::string Locale::compact(std::int64_t n) const {
  if (n < 10000) return number(n);
  char buf[32];
  if (n < 1000000) {
    std::snprintf(buf, sizeof buf, "%.1fk", static_cast<double>(n) / 1000.0);
  } else {
    std::snprintf(buf, sizeof buf, "%.1fM", static_cast<double>(n) / 1000000.0);
  }
  std::string out = buf;
  for (char& c : out) {
    if (c == ',') c = '.';
  }
  return lang_ == Lang::En ? out : with_decimal_comma(out);
}

const char* Locale::noun(std::int64_t n, const Noun& noun) const {
  const std::uint64_t m = n < 0 ? 0 - static_cast<std::uint64_t>(n) : static_cast<std::uint64_t>(n);
  if (lang_ == Lang::Ru) {
    const std::uint64_t last = m % 10;
    const std::uint64_t last2 = m % 100;
    if (last == 1 && last2 != 11) return noun.one;
    if (last >= 2 && last <= 4 && (last2 < 12 || last2 > 14)) return noun.few;
    return noun.many;
  }
  return m == 1 ? noun.one : noun.many;
}

std::string Locale::count(std::int64_t n, const Noun& noun) const {
  return number(n) + " " + this->noun(n, noun);
}

const char* Locale::weekday_short(int weekday) const {
  return weekday >= 0 && weekday < 7 ? kWeekdayShort[index_of(lang_)][weekday] : "???";
}

const char* Locale::weekday(int weekday) const {
  return weekday >= 0 && weekday < 7 ? kWeekday[index_of(lang_)][weekday] : "???";
}

const char* Locale::weekday_in_sentence(int weekday) const {
  return weekday >= 0 && weekday < 7 ? kWeekdayInSentence[index_of(lang_)][weekday] : "???";
}

const char* Locale::month_short(unsigned month) const {
  return month >= 1 && month <= 12 ? kMonthShort[index_of(lang_)][month - 1] : "???";
}

std::string Locale::day_month(std::int64_t day, bool with_year) const {
  const Civil c = civil_from_days(day);
  const std::string year = std::to_string(c.year);
  switch (lang_) {
    case Lang::Tr:
      return std::to_string(c.day) + " " + month_short(c.month) + (with_year ? " " + year : "");
    case Lang::Ru:
      return std::to_string(c.day) + " " + kRussianMonthOfDate[c.month - 1] +
             (with_year ? " " + year : "");
    case Lang::En: break;
  }
  return std::string(month_short(c.month)) + " " + std::to_string(c.day) +
         (with_year ? ", " + year : "");
}

std::string Locale::date(std::int64_t day) const { return day_month(day, true); }

std::string Locale::month_year(std::int64_t day) const {
  const Civil c = civil_from_days(day);
  return std::string(month_short(c.month)) + " " + std::to_string(c.year);
}

std::string Locale::span(std::int64_t days) const {
  if (days < 1) days = 1;
  const Strings& t = text();
  if (days < 60) return count(days, t.day);
  const auto months = static_cast<std::int64_t>(static_cast<double>(days) / 30.4375);
  const std::int64_t years = months / 12;
  const std::int64_t rest = months % 12;
  if (lang_ == Lang::Ru) {
    if (years == 0) return number(months) + " мес.";
    if (rest == 0) return count(years, t.year);
    return count(years, t.year) + " " + std::to_string(rest) + " мес.";
  }
  if (years == 0) return count(months, t.month);
  if (rest == 0) return count(years, t.year);
  if (lang_ == Lang::Tr) return count(years, t.year) + " " + count(rest, t.month);
  return std::to_string(years) + "y " + std::to_string(rest) + "m";
}

std::string Locale::span_prose(std::int64_t days) const {
  if (days < 1) days = 1;
  const Strings& t = text();
  if (days < 60) return count(days, t.day);
  const auto months = static_cast<std::int64_t>(static_cast<double>(days) / 30.4375);
  if (months < 24) return count(months, t.month);
  const std::int64_t years = months / 12;
  const std::int64_t rest = months % 12;
  std::string out = count(years, t.year);
  if (rest == 0) return out;
  switch (lang_) {
    case Lang::Tr: return out + " " + count(rest, t.month);
    case Lang::Ru: return out + " и " + count(rest, t.month);
    case Lang::En: break;
  }
  return out + " and " + count(rest, t.month);
}

std::string Locale::duration(std::int64_t seconds) const {
  if (seconds < 0) seconds = 0;
  if (seconds >= 2 * 86400) return count(seconds / 86400, text().day);
  const bool hours = seconds >= 3600;
  const std::string value = std::to_string(hours ? seconds / 3600 : seconds / 60);
  switch (lang_) {
    case Lang::Tr: return value + (hours ? " sa" : " dk");
    case Lang::Ru: return value + (hours ? " ч" : " мин");
    case Lang::En: break;
  }
  return value + (hours ? "h" : "m");
}

std::string Locale::language_name(const std::string& name) const {
  if (lang_ == Lang::En) return name;
  const bool tr = lang_ == Lang::Tr;
  if (name == "Other") return tr ? "Diğer" : "Другое";
  if (name == "Lockfiles") return tr ? "Kilit dosyaları" : "Lock-файлы";
  if (name == "Images") return tr ? "Görseller" : "Изображения";
  if (name == "Text") return tr ? "Metin" : "Текст";
  return name;
}

std::vector<TemplatePart> split_template(std::string_view tmpl) {
  std::vector<TemplatePart> parts;
  std::size_t literal = 0;
  std::size_t i = 0;
  while (i < tmpl.size()) {
    if (tmpl[i] == '{' && i + 2 < tmpl.size() && tmpl[i + 1] >= '0' && tmpl[i + 1] <= '9' &&
        tmpl[i + 2] == '}') {
      if (i > literal) parts.push_back({tmpl.substr(literal, i - literal), -1});
      parts.push_back({{}, tmpl[i + 1] - '0'});
      i += 3;
      literal = i;
    } else {
      ++i;
    }
  }
  if (literal < tmpl.size()) parts.push_back({tmpl.substr(literal), -1});
  return parts;
}

std::string fill(std::string_view tmpl, const std::vector<std::string>& args) {
  std::string out;
  for (const TemplatePart& part : split_template(tmpl)) {
    if (part.arg < 0) {
      out += part.text;
    } else if (static_cast<std::size_t>(part.arg) < args.size()) {
      out += args[static_cast<std::size_t>(part.arg)];
    }
  }
  return out;
}

}  // namespace gitstory
