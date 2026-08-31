#include "identity_lexicon.hpp"

namespace identity {
namespace {

// Slot sizes are chosen against the two-sided coverage test, not against the
// minimums: a head slot (surname, place, nature) drawn alone by a strategy
// needs ~40 entries before no single name exceeds 0.5% of a 100k sample.

// ---- Turkish. Reviewed by a native speaker. -------------------------------
// Surnames are ORDINARY surnames: seeding this with conglomerate names
// (Koç, Sabancı) generates real firms.
const char* const kTrSurname[] = {
    "Demir","Kaya","Yılmaz","Şahin","Çelik","Yıldız","Aydın","Özdemir","Arslan",
    "Kılıç","Çetin","Korkmaz","Karaca","Polat","Öztürk","Şimşek","Tekin","Bulut",
    "Güneş","Ateş","Toprak","Akın","Uysal","Erdem","Aksoy","Barut","Cengiz",
    "Duran","Ergin","Fidan","Gürsoy","Gürbüz","Kandemir","Sezer","Turan","Yavuz",
    "Doğan","Kurt","Balcı","Çakır","Eren","Sarıkaya"};
const char* const kTrMaterial[] = {
    "Demir","Çelik","Cam","Şişe","Boru","Taş","Kum","Çimento","Bakır","Kağıt",
    "İplik","Dokuma","Tuğla","Maden","Kömür","Petrol","Kimya","Plastik",
    "Alüminyum","Mermer"};
const char* const kTrNature[] = {
    "Yıldız","Güneş","Deniz","Dağ","Kartal","Şahin","Aslan","Meşe","Çınar",
    "Gül","Lale","Pınar","Irmak","Toprak","Rüzgar","Şafak","Umut","Ufuk","Doruk",
    "Zirve","Bozkır","Yamaç","Kaynak","Bulut","Orman","Ova","Şelale","Kaya",
    "Fırtına","Alev","Kıvılcım","Nehir","Liman","Yaprak","Tohum","Filiz","Ada",
    "Kıyı","Turna","Ceylan"};
const char* const kTrQual[] = {
    "Ak","Kara","Al","Yeni","Büyük","Ulu","Öz","Tek","Can","Er","Baş","Gür",
    "Sarp","Yüce","Bir","Genç"};
const char* const kTrPlace[] = {
    "Anadolu","Marmara","Ege","Akdeniz","Karadeniz","Toros","Boğaziçi","Trakya",
    "Kapadokya","Çukurova","Konya","Bursa","Aydın","Menderes","Fırat","Dicle",
    "Sakarya","Kızılırmak","Yeşilırmak","Uludağ","Erciyes","Ağrı","Van","Kayseri",
    "Denizli","Manisa","Balıkesir","Eskişehir","Adana","Mersin","Antalya",
    "Samsun","Trabzon","Rize","Edirne","Kütahya","Malatya","Sivas","Nemrut",
    "Beydağı"};
const char* const kTrSector[] = {
    "Sanayi","Ticaret","Holding","Bankası","Denizcilik","Enerji","İnşaat",
    "Tekstil","Gıda","Otomotiv","Lojistik","Madencilik","Kimya","Makina"};
const char* const kTrLegal[] = {"A.Ş.","Ltd. Şti.","Holding"};

// ---- English. UNVERIFIED. -------------------------------------------------
const char* const kEnSurname[] = {
    "Ashcroft","Barrow","Calder","Danforth","Eastwood","Fairbank","Garrick",
    "Halstead","Ingram","Jarvis","Kendrick","Larkin","Merrick","Norwood",
    "Osgood","Prescott","Quillan","Radcliffe","Selwyn","Thornton","Upton",
    "Vickers","Wetherby","Yarrow","Blackwood","Corwin","Draycott","Emberly",
    "Fenwick","Grimshaw","Hawksley","Isherwood","Kingsley","Loxley","Marlowe",
    "Newcombe","Padgett","Redmayne","Stanhope","Trevelyan","Wainwright","Ellery"};
const char* const kEnMaterial[] = {
    "Iron","Steel","Glass","Copper","Timber","Stone","Sand","Cement","Paper",
    "Cotton","Brick","Coal","Slate","Bronze","Linen","Resin","Alloy","Granite",
    "Pewter","Clay"};
const char* const kEnNature[] = {
    "Star","Sun","Sea","Mountain","Eagle","Falcon","Lion","Oak","Elm","Rose",
    "Spring","River","Earth","Wind","Dawn","Summit","Ridge","Meadow","Cloud",
    "Forest","Harbour","Cascade","Rock","Storm","Flame","Ember","Delta","Leaf",
    "Seed","Sprout","Isle","Shore","Heron","Stag","Hawk","Bramble","Thistle",
    "Fern","Willow","Beacon"};
const char* const kEnQual[] = {
    "White","Black","Red","New","Great","Grand","Prime","Sole","True","First",
    "Chief","Bright","Steady","Noble","Broad","Keen"};
const char* const kEnPlace[] = {
    "Northgate","Southwold","Eastmere","Westford","Kingsbridge","Highmoor",
    "Blackwater","Stonebrook","Fairhaven","Redhill","Ashford","Bellwater",
    "Carrick","Downfield","Elmsworth","Foxholme","Greenlaw","Hartwell",
    "Ilkmoor","Jesmond","Kirkstall","Langdale","Marchwood","Netherby","Oakvale",
    "Penrose","Ravensmoor","Sedgely","Tarnwick","Underhill","Vale","Wroxham",
    "Yardley","Aldermere","Brentmoor","Corbridge","Dunbarrow","Ferncliff",
    "Glenmere","Holloway"};
const char* const kEnSector[] = {
    "Industries","Trading","Holdings","Bank","Shipping","Energy",
    "Construction","Textiles","Foods","Motors","Logistics","Mining",
    "Chemicals","Engineering"};
const char* const kEnLegal[] = {"Ltd.","PLC","& Co."};

// ---- Japanese, romanized. UNVERIFIED. -------------------------------------
const char* const kJaSurname[] = {
    "Tanaka","Suzuki","Nakamura","Yamamoto","Kobayashi","Watanabe","Saito",
    "Kato","Yoshida","Yamada","Sasaki","Matsumoto","Inoue","Kimura","Hayashi",
    "Shimizu","Yamazaki","Mori","Ikeda","Hashimoto","Ishikawa","Ogawa","Goto",
    "Okada","Hasegawa","Murakami","Kondo","Ishii","Sakamoto","Endo","Aoki",
    "Fujita","Nishimura","Fukuda","Ota","Miura","Fujiwara","Okamoto","Matsuda",
    "Nakagawa","Harada","Ono"};
const char* const kJaMaterial[] = {
    "Tetsu","Hagane","Garasu","Do","Mokuzai","Ishi","Suna","Semento","Kami",
    "Wata","Renga","Sekitan","Suraito","Seido","Asa","Jushi","Gokin","Mikage",
    "Suzu","Nendo"};
const char* const kJaNature[] = {
    "Hoshi","Taiyo","Tsuki","Umi","Yama","Taka","Washi","Matsu","Sakura","Kaze",
    "Kawa","Mori","Iwa","Yuki","Hikari","Sora","Nami","Take","Ume","Kumo",
    "Shima","Minato","Tani","Hara","Izumi","Nagare","Kiri","Ame","Niji",
    "Kaminari","Honoo","Tsuchi","Kishi","Tsuru","Kame","Shika","Hayate","Asahi",
    "Kaede","Sugi"};
const char* const kJaQual[] = {
    "Shin","Dai","Ko","Chu","Sei","Toa","Zen","Ichi","Hon","Kyo","Ryo","Sho",
    "Tai","Ten","Ei","Ko-a"};
const char* const kJaPlace[] = {
    "Kanto","Kansai","Tohoku","Kyushu","Shikoku","Hokuriku","Chubu","Sanyo",
    "Setouchi","Tokai","Musashino","Yamashiro","Omi","Kai","Shinano","Echigo",
    "Dewa","Mutsu","Hida","Iyo","Tosa","Bizen","Harima","Awaji","Sagami","Kazusa",
    "Kozuke","Owari","Mikawa","Ise","Tanba","Wakasa","Noto","Etchu","Iwami",
    "Izumo","Hoki","Bungo","Hyuga","Satsuma"};
const char* const kJaSector[] = {
    "Kogyo","Shoji","Holdings","Ginko","Kaiun","Denryoku","Kensetsu","Boseki",
    "Shokuhin","Jidosha","Butsuryu","Kozan","Kagaku","Seiki"};
const char* const kJaLegal[] = {"K.K.", "G.K.", "Group"};

// ---- German. UNVERIFIED. --------------------------------------------------
const char* const kDeSurname[] = {
    "Bauer","Fischer","Weber","Schneider","Wagner","Becker","Hoffmann","Keller",
    "Lehmann","Neumann","Richter","Schuster","Vogel","Winkler","Zimmermann",
    "Brandt","Engel","Frank","Gerber","Haas","Jung","Kramer","Lang","Maurer",
    "Nagel","Ostermann","Pfeiffer","Reuter","Sauer","Thiel","Ulrich","Voigt",
    "Wendel","Ziegler","Albrecht","Bergmann","Dietrich","Ehlers","Fuchs",
    "Grunewald","Hartmann","Iserlohn"};
const char* const kDeMaterial[] = {
    "Eisen","Stahl","Glas","Kupfer","Holz","Stein","Sand","Zement","Papier",
    "Baumwolle","Ziegel","Kohle","Schiefer","Bronze","Leinen","Harz","Legierung",
    "Granit","Zinn","Ton"};
const char* const kDeNature[] = {
    "Stern","Sonne","Meer","Berg","Adler","Falke","Löwe","Eiche","Ulme","Rose",
    "Quelle","Fluss","Erde","Wind","Morgen","Gipfel","Grat","Wiese","Wolke",
    "Wald","Hafen","Fels","Sturm","Flamme","Funke","Blatt","Saat","Trieb",
    "Insel","Ufer","Reiher","Hirsch","Habicht","Distel","Farn","Weide","Linde",
    "Buche","Tanne","Birke"};
const char* const kDeQual[] = {
    "Weiß","Schwarz","Rot","Neu","Groß","Hoch","Erst","Fest","Klar","Stark",
    "Rein","Frei","Weit","Alt","Gut","Kühn"};
const char* const kDePlace[] = {
    "Rheinland","Westfalen","Bayern","Sachsen","Schwaben","Franken","Hessen",
    "Thüringen","Vogtland","Oberland","Altmark","Breisgau","Eifel","Harz",
    "Odenwald","Spessart","Taunus","Wetterau","Allgäu","Bergstraße","Donau",
    "Elbland","Fichtelberg","Grabfeld","Havelland","Isartal","Kraichgau","Lausitz",
    "Markgraf","Neckartal","Ostsee","Pfalz","Ruhrtal","Saartal","Uckermark",
    "Weserbergland","Emsland","Nordmark","Mosel","Ahrtal"};
const char* const kDeSector[] = {
    "Industrie","Handel","Holding","Bank","Reederei","Energie","Bau","Textil",
    "Nahrung","Motoren","Logistik","Bergbau","Chemie","Maschinenbau"};
const char* const kDeLegal[] = {"AG","GmbH","KGaA"};

const Lexicon kTable[] = {
    {kTrSurname, kTrMaterial, kTrNature, kTrQual, kTrPlace, kTrSector, kTrLegal, true},
    {kEnSurname, kEnMaterial, kEnNature, kEnQual, kEnPlace, kEnSector, kEnLegal, false},
    {kJaSurname, kJaMaterial, kJaNature, kJaQual, kJaPlace, kJaSector, kJaLegal, false},
    {kDeSurname, kDeMaterial, kDeNature, kDeQual, kDePlace, kDeSector, kDeLegal, false},
};

}  // namespace

const Lexicon& lexicon(Locale l) { return kTable[static_cast<uint8_t>(l)]; }

const char* locale_name(Locale l) {
    static const char* const kNames[] = {"Turkish", "English", "Japanese", "German"};
    return kNames[static_cast<uint8_t>(l)];
}

}  // namespace identity
