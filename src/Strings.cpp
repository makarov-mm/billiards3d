#include "Strings.h"

#include <array>
#include <cstddef>
#include <utility>

namespace {

struct Entry {
    Str id;
    std::string_view english;
    std::string_view russian;
};

constexpr std::array<Entry, static_cast<std::size_t>(Str::Count)> Table{{
    {Str::Title, "3D Billiards", "Бильярд 3D"},
    {Str::Game, "Game", "Игра"},
    {Str::GameEight, "Eight-ball", "Восьмёрка"},
    {Str::GameNine, "Nine-ball", "Девятка"},
    {Str::GameAmericanka, "Free pyramid", "Американка"},
    {Str::Mode, "Mode", "Режим"},
    {Str::ModeComputer, "Vs computer", "С компьютером"},
    {Str::ModeTwoPlayers, "Two players", "Два игрока"},
    {Str::ModePractice, "Practice", "Тренировка"},
    {Str::ModeDemo, "Demo game", "Демонстрация"},
    {Str::Level, "Level", "Уровень"},
    {Str::Level1, "Novice", "Новичок"},
    {Str::Level2, "Beginner", "Начинающий"},
    {Str::Level3, "Amateur", "Любитель"},
    {Str::Level4, "Strong", "Сильный"},
    {Str::Level5, "Master", "Мастер"},
    {Str::Aim, "Aim lines", "Прицел"},
    {Str::AimFull, "Full", "Полный"},
    {Str::AimShort, "Short", "Короткий"},
    {Str::AimOff, "Off", "Нет"},
    {Str::Theme, "Theme", "Тема"},
    {Str::Sound, "Sound", "Звук"},
    {Str::LanguageName, "English", "Русский"},
    {Str::Camera, "View", "Вид"},
    {Str::CameraOverview, "Overview", "Обзор"},
    {Str::CameraBehind, "Behind the ball", "За шаром"},
    {Str::CameraTop, "From above", "Сверху"},
    {Str::NewGame, "New game", "Новая партия"},
    {Str::UndoShot, "Undo shot", "Отменить удар"},
    {Str::NameYou, "You", "Вы"},
    {Str::NameComputer, "Computer", "Компьютер"},
    {Str::NamePlayer, "Player {}", "Игрок {}"},
    {Str::NameComputerN, "Computer {}", "Компьютер {}"},
    {Str::ShotYou, "Your shot", "Ваш удар"},
    {Str::ShotComputer, "Computer's shot", "Удар компьютера"},
    {Str::ShotPlayer, "Player {} to shoot", "Удар игрока {}"},
    {Str::ShotComputerN, "Computer {} to shoot", "Удар компьютера {}"},
    {Str::Practice, "Practice", "Тренировка"},
    {Str::WinYou, "You win!", "Вы победили!"},
    {Str::WinComputer, "The computer wins", "Победил компьютер"},
    {Str::WinPlayer, "Player {} wins!", "Победил игрок {}!"},
    {Str::WinComputerN, "Computer {} wins", "Победил компьютер {}"},
    {Str::TableCleared, "Table cleared!", "Стол чист!"},
    {Str::BreakYou, "You break", "Вы разбиваете"},
    {Str::BreakComputer, "The computer breaks", "Разбивает компьютер"},
    {Str::BreakPlayer, "Player {} breaks", "Разбивает игрок {}"},
    {Str::BreakComputerN, "Computer {} breaks", "Разбивает компьютер {}"},
    {Str::Breaking, "Break the rack", "Разбой пирамиды"},
    {Str::Thinking, "Thinking…", "Думает…"},
    {Str::Aiming, "Taking aim…", "Прицеливается…"},
    {Str::Rolling, "The balls are rolling", "Шары катятся"},
    {Str::GameOver, "Game over", "Партия окончена"},
    {Str::PressNew, "N or «New game» to play again", "N или «Новая партия» — сыграть ещё"},
    {Str::BallInHand, "Ball in hand: place the cue ball", "Шар с руки: поставьте биток"},
    {Str::BallInKitchen, "Ball in hand behind the line", "Шар с руки из «дома», за линией"},
    {Str::OpenTable, "Open table", "Стол открыт"},
    {Str::Solids, "Solids", "Сплошные"},
    {Str::Stripes, "Stripes", "Полосатые"},
    {Str::OnEight, "The 8 to the {}", "Восьмёрка: {}"},
    {Str::HitLowest, "Hit the {} first", "Сначала — шар {}"},
    {Str::AnyBall, "Any ball at any ball", "Любой шар по любому"},
    {Str::Pocket0, "near left corner", "ближняя левая угловая"},
    {Str::Pocket1, "near side pocket", "ближняя средняя"},
    {Str::Pocket2, "near right corner", "ближняя правая угловая"},
    {Str::Pocket3, "far left corner", "дальняя левая угловая"},
    {Str::Pocket4, "far side pocket", "дальняя средняя"},
    {Str::Pocket5, "far right corner", "дальняя правая угловая"},
    {Str::CueBall, "cue ball", "биток"},
    {Str::Foul, "Foul: {}", "Фол: {}"},
    {Str::FoulScratch, "the cue ball went in", "биток в лузе"},
    {Str::FoulMiss, "no ball was hit", "промах по шарам"},
    {Str::FoulWrongBall, "the wrong ball was hit first", "первым задет не тот шар"},
    {Str::FoulNoRail, "no ball reached a cushion", "ни один шар не коснулся борта"},
    {Str::FoulEightEarly, "the 8 went in too early", "восьмёрка забита раньше времени"},
    {Str::FoulEightWrongPocket, "the 8 went into the wrong pocket", "восьмёрка не в ту лузу"},
    {Str::FoulEightOnFoul, "the 8 went in on a foul", "восьмёрка забита с фолом"},
    {Str::LogPotted, "{}: potted {}", "{}: забит {}"},
    {Str::LogPottedMany, "{}: potted {}", "{}: забиты {}"},
    {Str::LogNothing, "{}: nothing potted", "{}: без забитых"},
    {Str::LogFoul, "{}: foul, {}", "{}: фол, {}"},
    {Str::LogSolids, "{}: solids", "{}: сплошные"},
    {Str::LogStripes, "{}: stripes", "{}: полосатые"},
    {Str::LogBack, "Back on the table: {}", "Возвращены на стол: {}"},
    {Str::LogWin, "{} won the game", "{}: победа"},
    {Str::Power, "Power", "Сила"},
    {Str::Spin, "Spin", "Винт"},
    {Str::History, "Shots", "Ход партии"},
    {Str::NoShotsYet, "No shots yet", "Ударов ещё не было"},
    {Str::Shots, "{} shots", "Ударов: {}"},
    {Str::Left, "{} left", "Осталось: {}"},
    {Str::ToWin, "of {}", "из {}"},
    {Str::Footer, "F1 help · Tab panel · F11 full screen", "F1 справка · Tab панель · F11 весь экран"},
    {Str::Controls, "Controls", "Управление"},
    {Str::KeyMove, "Mouse", "Мышь"},
    {Str::KeyShiftMove, "Shift + mouse", "Shift + мышь"},
    {Str::KeyPull, "Press, pull back", "Нажать и оттянуть"},
    {Str::KeyClick, "Click ball / pocket", "Щелчок: шар / луза"},
    {Str::KeyDragCue, "Drag the cue ball", "Тащить биток"},
    {Str::KeyRightDrag, "Right drag", "Правая кнопка"},
    {Str::KeyWheel, "Wheel", "Колесо мыши"},
    {Str::HelpAim, "Aim at the point", "Прицел в точку"},
    {Str::HelpFineAim, "Fine aim", "Точная наводка"},
    {Str::HelpStrike, "Strike: farther is harder", "Удар: дальше — сильнее"},
    {Str::HelpClick, "Pick the ball or call the pocket", "Выбрать шар или лузу"},
    {Str::HelpPlace, "Place a ball in hand", "Поставить шар с руки"},
    {Str::HelpRotate, "Turn the view", "Повернуть вид"},
    {Str::HelpZoom, "Zoom in and out", "Приблизить и отдалить"},
    {Str::HelpShoot, "Strike with the power set", "Удар с заданной силой"},
    {Str::HelpPower, "Power", "Сила удара"},
    {Str::HelpTurn, "Turn the aim (Shift: finer)", "Поворот прицела (Shift — точнее)"},
    {Str::HelpSpin, "Spin: where the cue strikes", "Винт: куда бьёт кий"},
    {Str::HelpCenter, "Strike the middle", "Удар в центр"},
    {Str::HelpCamera, "Next view", "Следующий вид"},
    {Str::HelpGuide, "Aim lines", "Прицельные линии"},
    {Str::HelpUndo, "Undo the shot", "Отменить удар"},
    {Str::HelpNew, "New game", "Новая партия"},
    {Str::HelpGame, "Next game", "Следующая игра"},
    {Str::HelpMode, "Who plays", "Кто играет"},
    {Str::HelpLevel, "Computer level", "Уровень компьютера"},
    {Str::HelpTheme, "Next theme", "Следующая тема"},
    {Str::HelpSound, "Sound on or off", "Звук вкл/выкл"},
    {Str::HelpLanguage, "English / Russian", "Русский / английский"},
    {Str::HelpPanel, "Show or hide the panel", "Показать/скрыть панель"},
    {Str::HelpFullScreen, "Full screen", "Весь экран"},
    {Str::HelpClose, "Close this help", "Закрыть справку"},
    {Str::Rules, "Rules", "Правила"},
    {Str::RulesEight,
     "Eight-ball. The first ball potted after the break gives the shooter a group: solids (1–7) or stripes (9–15). Hit a ball of your group "
     "first, then pot a ball or send one to a cushion. Potting your balls keeps the turn. With the group cleared, pot the 8 in the pocket called. "
     "The 8 potted early or on a foul loses. After a foul the opponent has ball in hand.",
     "Восьмёрка. Первый шар, забитый после разбоя, даёт группу: сплошные (1–7) или полосатые (9–15). Первым касайтесь шара своей группы, "
     "затем забейте шар или доведите любой до борта. Забили свой — бьёте снова. Очистив группу, забейте восьмёрку в заказанную лузу. "
     "Восьмёрка, забитая раньше или с фолом, — поражение. После фола соперник получает шар с руки."},
    {Str::RulesNine,
     "Nine-ball. The cue ball must first hit the lowest ball on the table. Any ball potted on a fair shot keeps the turn, and the 9 potted on a "
     "fair shot wins, even early. A shot that pots nothing must send a ball to a cushion. After a foul the opponent has ball in hand anywhere.",
     "Девятка. Биток должен первым коснуться шара с наименьшим номером. Любой шар, забитый без фола, даёт следующий удар, а девятка, забитая "
     "без фола, — победу, даже раньше времени. Если ничего не забито, какой-то шар должен коснуться борта. После фола соперник ставит биток "
     "куда угодно."},
    {Str::RulesAmericanka,
     "Free pyramid. Sixteen balls on a big table with tight pockets. After the break any ball may be struck at any other; every ball potted on "
     "a fair shot counts, the struck one too, and the shooter goes on. Hitting nothing, or potting nothing with no ball reaching a cushion, is a "
     "foul: one of the shooter's balls goes back on the table. Eight balls win.",
     "Американка. Шестнадцать шаров на большом столе с узкими лузами. После разбоя можно бить любым шаром по любому; каждый шар, забитый без "
     "фола, засчитывается, в том числе биток, и удар повторяется. Промах или удар без забитого шара, после которого ни один шар не коснулся "
     "борта, — фол: один из шаров нарушителя возвращается на стол. Побеждает забивший восемь."},
    {Str::NothingToUndo, "No shots to take back", "Нечего отменять"},
    {Str::NotYourBall, "That ball may not be struck now", "Этим шаром сейчас бить нельзя"},
    {Str::KitchenOnly, "Only behind the head string", "Только в «доме», за линией"},
    {Str::CallPocket, "Click a pocket to call it for the 8", "Щёлкните по лузе, чтобы заказать её"},
}};

constexpr bool InOrder()
{
    for (std::size_t i = 0; i < Table.size(); ++i)
        if (std::to_underlying(Table[i].id) != static_cast<int>(i))
            return false;
    return true;
}
static_assert(InOrder(), "the table lists the strings in the order of Str");

} // namespace

std::string_view Tr(Str id, Language language)
{
    const Entry& entry = Table[static_cast<std::size_t>(std::to_underlying(id))];
    return language == Language::Russian ? entry.russian : entry.english;
}
