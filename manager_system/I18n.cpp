/**
 * ============================================================
 *  I18n — English / Persian string table (implementation)
 * ============================================================
 *  One lookup rule: the key IS the English literal. The FA column
 *  mirrors it line-for-line, so FA output only ever swaps text,
 *  never flow control. ASCII digits and dates stay ASCII on
 *  purpose — numeric columns keep aligning in every terminal.
 * ============================================================
 */
#include "I18n.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace i18n
{
namespace
{

struct Pair
{
    const char *en;
    const char *fa;
};

/* The interaction layer, screen by screen (mainApp.cpp render
 * order + the facade's income report). Keys must match the code
 * byte-for-byte, including leading spaces and \n. */
const Pair kTable[] = {
    {"        Console edition | C++17 | hand-built "
     "data structures\n\n",
     "        نسخهٔ کنسولی | C++17 | "
     "ساختمان‌داده‌های "
     "دست‌ساز\n\n"},
    {"  Please enter a whole number.\n", "  لطفاً یک عدد صحیح وارد "
                                         "کنید.\n"},
    {"  Please enter a number.\n", "  لطفاً یک عدد وارد کنید.\n"},
    {"  Invalid date - use YYYY-MM-DD (e.g. "
     "2026-11-09).\n",
     "  تاریخ نامعتبر است — از "
     "قالب YYYY-MM-DD استفاده کنید "
     "(مثلاً 2026-11-09).\n"},
    {"  Choice must be between ", "  انتخاب باید بین "},
    {" and ", " تا "},
    {"NAME", "نام"},
    {"BRAND", "برند"},
    {"PRICE/DAY", "قیمت/روز"},
    {"STATUS", "وضعیت"},
    {"QUEUE", "صف"},
    {"AVAILABLE", "آزاد"},
    {"MAINTENANCE", "تعمیرات"},
    {"RENTED", "اجاره‌شده"},
    {" booking(s)", " رزرو"},
    {"\n  The fleet is empty. Ask the manager to add "
     "cars.\n",
     "\n  ناوگان خالی است. از مدیر "
     "بخواهید خودرو اضافه کند.\n"},
    {"  FLEET (", "  ناوگان ("},
    {" cars", " خودرو"},
    {", sorted by name", "، مرتب بر اساس نام"},
    {", insertion order", "، ترتیب افزودن"},
    {"  [series #", "  [سری #"},
    {"\n  You have no reservations yet.\n", "\n  هنوز رزروی ندارید.\n"},
    {"\n  YOUR RESERVATIONS\n", "\n  رزروهای من\n"},
    {" day(s))", " روز)"},
    {" day(s))\n", " روز)\n"},
    {"\n  You are not waiting for any car.\n", "\n  در صف انتظار خودرویی "
                                               "نیستید.\n"},
    {"\n  YOUR WAITLIST\n", "\n  صف انتظار من\n"},
    {"  wish #", "  درخواست #"},
    {"  We auto-book you the moment a wished range "
     "frees up.\n",
     "  به‌محض آزادشدن بازهٔ "
     "موردنظر، خودکار برایتان "
     "رزرو می‌کنیم.\n"},
    {"\n--- Customer Registration ---\n", "\n--- ثبت‌نام مشتری ---\n"},
    {"  Username: ", "  نام کاربری: "},
    {"  Password (min 4 chars): ", "  رمز عبور (حداقل 4 نویسه): "},
    {"  Account created. You can log in now.\n", "  حساب ساخته شد. حالا "
                                                 "می‌توانید وارد شوید.\n"},
    {"  Username already taken.\n", "  این نام کاربری قبلاً "
                                    "گرفته شده است.\n"},
    {"  Password must be at least 4 characters.\n", "  رمز عبور باید حداقل 4 "
                                                    "نویسه باشد.\n"},
    {"\n--- Login ---\n", "\n--- ورود ---\n"},
    {"  Password: ", "  رمز عبور: "},
    {"  Welcome, ", "  خوش آمدید، "},
    {"Customer", "مشتری"},
    {"Staff", "کارمند"},
    {"Manager", "مدیر"},
    {"  This account is blocked. Contact the manager.\n", "  این حساب مسدود شده است. با"
                                                          " مدیر تماس بگیرید.\n"},
    {"  Wrong password. 3 failed attempts block the "
     "account!\n",
     "  رمز عبور اشتباه است. 3 "
     "ورود ناموفق حساب را مسدود "
     "می‌کند!\n"},
    {"  No such user.\n", "  چنین کاربری وجود ندارد.\n"},
    {"  Estimate: ", "  برآورد: "},
    {" day(s) x ", " روز × "},
    {"\n  Pay on delivery about ", "\n  پرداخت هنگام تحویل حدود "},
    {" — late fees are never discounted.\n", " — جریمهٔ دیرکرد هرگز "
                                             "تخفیف نمی‌گیرد.\n"},
    {"\n===== LOYALTY CARD | ", "\n===== کارت وفاداری | "},
    {"\n  Rank: ", "\n  رتبه: "},
    {"Bronze", "برنزی"},
    {"Silver", "نقره‌ای"},
    {"Gold", "طلایی"},
    {"Platinum", "پلاتینیوم"},
    {"  You hold the top rank of the program.\n", "  بالاترین رتبهٔ برنامه را "
                                                  "در اختیار دارید.\n"},
    {" points to ", " امتیاز مانده تا "},
    {"  Points available  : ", "  امتیازِ موجود     : "},
    {"\n  Lifetime points   : ", "\n  امتیاز کل برنامه  : "},
    {"\n  Completed rentals : ", "\n  اجاره‌های تکمیل‌شده: "},
    {"\n  Earning rate      : x", "\n  نرخ کسب امتیاز    : x"},
    {" - 1 point per 10.00 spent, credited on return\n "
     " Redemption        : 10 points = 1.00 off your "
     "bill (min 50, steps of 10)\n",
     " - به‌ازای هر 10.00 هزینه، 1 "
     "امتیاز؛ در زمان بازگشت "
     "اعمال می‌شود\n  بازخرید       "
     "     : 10 امتیاز = 1.00 تخفیف قبض "
     "(حداقل 50، گام‌های 10)\n"},
    {"  1. View fleet\n  2. Search car by name\n  3. My "
     "reservations\n  4. Make a reservation\n  5. Find "
     "cars free for a date range\n  6. Join a waitlist\n"
     "  7. My waitlist (leave)\n  8. Renew a rental\n  "
     "9. Pay my bill\n 10. My loyalty card\n 11. Redeem "
     "points on my bill\n 12. Logout\n 13. Change a "
     "booking (move dates)\n",
     "  1. مشاهدهٔ ناوگان\n  2. "
     "جست‌وجوی خودرو با نام\n  3. "
     "رزروهای من\n  4. ثبت رزرو\n  5. "
     "خودروهای آزاد در یک بازه\n  "
     "6. عضویت در صف انتظار\n  7. صف "
     "انتظار من (خروج)\n  8. تمدید "
     "اجاره\n  9. پرداخت قبض\n 10. "
     "کارت وفاداری\n 11. بازخرید "
     "امتیاز از قبض\n 12. خروج از "
     "حساب\n 13. تغییر یک رزرو "
     "(جابه‌جایی تاریخ)\n"},
    {"\n===== CUSTOMER MENU | ", "\n===== منوی مشتری | "},
    {" | balance due: ", " | بدهی: "},
    {"  Choice: ", "  انتخاب: "},
    {"  Car name: ", "  نام خودرو: "},
    {"\n  FOUND:\n", "\n  یافت شد:\n"},
    {"  No car named \"", "  خودرویی به نام «"},
    {"\".\n", "» یافت نشد.\n"},
    {"  Not a series id.\n", "  شناسهٔ سری نامعتبر است.\n"},
    {"  Cancel a whole series? enter its #id (empty = "
     "stay): ",
     "  لغو کل سری؟ شناسهٔ # آن را "
     "وارد کنید (خالی = ماندن): "},
    {" upcoming booking(s) cancelled.\n", " رزرو پیش‌رو لغو شد.\n"},
    {"  Start date (YYYY-MM-DD): ", "  تاریخ شروع (YYYY-MM-DD): "},
    {"  End date   (YYYY-MM-DD): ", "  تاریخ پایان (YYYY-MM-DD): "},
    {"  Repeat: 0 none, 1 weekly, 2 biweekly, 3 "
     "monthly: ",
     "  تکرار: 0 هیچ، 1 هفتگی، 2 "
     "دوهفتگی، 3 ماهانه: "},
    {"  Reservation confirmed! Pay on delivery.\n", "  رزرو تأیید شد! پرداخت "
                                                    "هنگام تحویل.\n"},
    {"  Reservation failed: ", "  خطای رزرو: "},
    {"  Tip: join the waitlist (option 6) and we book "
     "you\n       automatically when the range frees "
     "up.\n",
     "  راهنما: به صف انتظار "
     "بپیوندید (گزینهٔ 6) تا "
     "به‌محض آزادشدن\n       بازه، "
     "خودکار رزرو شوید.\n"},
    {"  Choice must be between 0 and 3.\n", "  انتخاب باید بین 0 و 3 باشد.\n"},
    {"  Book it how many times (2-8): ", "  چند بار رزرو شود (2-8): "},
    {" booked: ", " رزرو شد: "},
    {"weekly", "هفتگی"},
    {"biweekly", "دوهفتگی"},
    {"monthly", "ماهانه"},
    {" x ", " × "},
    {" on ", " برای «"},
    {"    pickup ", "    دریافت "},
    {"  Estimate per pickup: ", "  برآورد هر دریافت: "},
    {" after ", " پس از "},
    {"\n  Series total about ", "\n  جمع کل سری حدود "},
    {" (late fees are never discounted).\n", " (جریمهٔ دیرکرد هرگز تخفیف "
                                             "نمی‌گیرد).\n"},
    {"  The whole series is cancelled at once from\n   "
     "    My reservations (option 3) — just give its"
     " #id.\n",
     "  کل سری یک‌جا از رزروهای "
     "من (گزینهٔ 3) لغو می‌شود —\n "
     "      فقط شناسهٔ # آن را "
     "بدهید.\n"},
    {"  Series booking failed: ", "  خطای رزرو سری: "},
    {"  Desired start (YYYY-MM-DD): ", "  شروع موردنظر (YYYY-MM-DD): "},
    {"  Desired end   (YYYY-MM-DD): ", "  پایان موردنظر (YYYY-MM-DD): "},
    {"  No car is free for ", "  هیچ خودرویی آزاد نیست در "
                              "بازهٔ "},
    {"\n  FREE FOR ", "\n  آزاد برای "},
    {" car(s))\n", " خودرو)\n"},
    {"  Tip: join a car's waitlist (option 6) to be\n  "
     "     auto-booked when a window opens up.\n",
     "  راهنما: به صف انتظار "
     "خودرو بپیوندید (گزینهٔ 6) "
     "تا\n       هنگام آزادشدن بازه،"
     " خودکار رزرو شوید.\n"},
    {"  You are on the waitlist for \"", "  در صف انتظار «"},
    {"\". We book you automatically when it frees up.\n",
     "» قرار گرفتید؛ به‌محض "
     "آزادشدن خودکار رزرو "
     "می‌شوید.\n"},
    {"  Waitlist failed: ", "  خطای صف انتظار: "},
    {"  Car to leave (empty = stay): ", "  خودروی خروج از صف (خالی = "
                                        "ماندن): "},
    {"  You left the waitlist of \"", "  از صف انتظار «"},
    {"  Extra days: ", "  روزهای اضافه: "},
    {"  Rental extended by ", "  اجاره تمدید شد به اندازهٔ"
                              " "},
    {" day(s). Extra charges apply at return.\n", " روز. هزینهٔ اضافه هنگام "
                                                  "بازگشت اعمال می‌شود.\n"},
    {"  Renewal failed: ", "  خطای تمدید: "},
    {"  Your balance is clear. Nothing to pay.\n", "  قبض شما تسویه است. چیزی "
                                                   "برای پرداخت نیست.\n"},
    {"  Receipt: paid ", "  رسید: پرداخت شد "},
    {". Thank you!\n", ". سپاسگزاریم!\n"},
    {"\n--- REDEEM LOYALTY POINTS ---\n", "\n--- بازخرید امتیاز وفاداری "
                                          "---\n"},
    {"  Points available: ", "  امتیاز موجود: "},
    {"  |  bill due: ", "  |  بدهی قبض: "},
    {"  Your bill is clear - points are redeemed "
     "against a bill,\n       so come back after your "
     "next rental.\n",
     "  قبض شما تسویه است — "
     "امتیاز فقط به قبض تعلق "
     "می‌گیرد؛\n       بعد از "
     "اجارهٔ بعدی برگردید.\n"},
    {"  Points to redeem (min 50, steps of 10): ", "  امتیاز برای بازخرید "
                                                   "(حداقل 50، گام‌های 10): "},
    {"  Credit of ", "  اعتبار "},
    {" applied. Bill now: ", " اعمال شد. قبض الان: "},
    {", points left: ", "، امتیاز باقی‌مانده: "},
    {"  Redemption failed: ", "  خطای بازخرید: "},
    {"  Logged out.\n", "  از حساب خارج شدید.\n"},
    {"\n--- CHANGE A BOOKING ---\n", "\n--- تغییر یک رزرو ---\n"},
    {"  Move a single booking (1) or a whole series "
     "(2): ",
     "  جابه‌جایی یک رزرو (1) یا "
     "کل سری (2): "},
    {"  Booking #id: ", "  شناسهٔ رزرو: "},
    {"  Not a booking id.\n", "  شناسهٔ رزرو نامعتبر است.\n"},
    {"  New start (YYYY-MM-DD): ", "  شروع جدید (YYYY-MM-DD): "},
    {"  New end   (YYYY-MM-DD): ", "  پایان جدید (YYYY-MM-DD): "},
    {"  Booking #", "  رزرو #"},
    {" moved to ", " جابه‌جا شد به "},
    {"  Move failed: ", "  خطای جابه‌جایی: "},
    {"  Series #id: ", "  شناسهٔ سری: "},
    {"  New date for the next pickup (YYYY-MM-DD): ", "  تاریخ جدید دریافت بعدی "
                                                      "(YYYY-MM-DD): "},
    {"  Series #", "  سری #"},
    {" upcoming booking(s) moved - reminders follow "
     "the new dates.\n",
     " رزرو پیش‌رو جابه‌جا شد —"
     " یادآورها از تاریخ جدید "
     "پیروی می‌کنند.\n"},
    {"  Choose 1 (single booking) or 2 (series).\n", "  1 (تک رزرو) یا 2 (سری) را "
                                                     "انتخاب کنید.\n"},
    {"\n===== STAFF MENU | ", "\n===== منوی کارمند | "},
    {"  1. View fleet\n  2. Search car by name\n  3. "
     "Deliver car to customer\n  4. Process car return "
     "(with penalty)\n  5. Send car to maintenance\n  6."
     " Finish maintenance\n  7. Show maintenance "
     "history\n  8. Serve waitlist now\n  9. Logout\n",
     "  1. مشاهدهٔ ناوگان\n  2. "
     "جست‌وجوی خودرو با نام\n  3. "
     "تحویل خودرو به مشتری\n  4. "
     "ثبت بازگشت خودرو (با جریمه)"
     "\n  5. ارسال به تعمیرات\n  6. "
     "پایان تعمیرات\n  7. نمایش "
     "سابقهٔ تعمیرات\n  8. سرویس صف"
     " انتظار\n  9. خروج از حساب\n"},
    {"  Car handed over. Drive safe!\n", "  خودرو تحویل شد. رانندگی "
                                         "ایمن!\n"},
    {"  Delivery failed: ", "  خطای تحویل: "},
    {"  Actual return date (YYYY-MM-DD): ", "  تاریخ واقعی بازگشت "
                                            "(YYYY-MM-DD): "},
    {"  Return processed and invoiced.\n", "  بازگشت ثبت و صورت‌حساب "
                                           "صادر شد.\n"},
    {"  Discount applied: ", "  تخفیف اعمال‌شده: "},
    {"  Rank up! ", "  ارتقای رتبه! "},
    {" - the new multiplier applies from the next "
     "rental.\n",
     " — ضریب جدید از اجارهٔ "
     "بعدی اعمال می‌شود.\n"},
    {"  Return failed: ", "  خطای بازگشت: "},
    {"  Date (YYYY-MM-DD): ", "  تاریخ (YYYY-MM-DD): "},
    {"  Description: ", "  توضیحات: "},
    {"  Cost: ", "  هزینه: "},
    {"  Car moved to MAINTENANCE. Log saved.\n", "  خودرو به تعمیرات رفت. در "
                                                 "سابقه ثبت شد.\n"},
    {"  Failed: ", "  خطا: "},
    {"  Failed: car not found or not in maintenance.\n", "  خطا: خودرو یافت نشد یا در "
                                                         "تعمیرات نیست.\n"},
    {"  Maintenance finished; car is back in service.\n", "  تعمیرات پایان یافت؛ "
                                                          "خودرو به ناوگان برگشت.\n"},
    {"  No maintenance recorded for this car.\n", "  برای این خودرو تعمیری ثبت"
                                                  " نشده است.\n"},
    {"\n  MAINTENANCE LOG | ", "\n  سابقهٔ تعمیرات | "},
    {" | cost ", " | هزینه "},
    {"  Booked ", "  رزرو شد برای "},
    {" from the waitlist.\n", " از صف انتظار.\n"},
    {"  Serve failed: ", "  خطای سرویس صف: "},
    {"\n  FLEET STATISTICS\n", "\n  آمار ناوگان\n"},
    {"Fleet size", "تعداد ناوگان"},
    {"Available", "آزاد"},
    {"Reserved", "رزروشده"},
    {"Rented", "اجاره‌شده"},
    {"Under maintenance", "در تعمیرات"},
    {"Active bookings", "رزروهای فعال"},
    {"Fleet utilization", "بهره‌وری ناوگان"},
    {"Gross income", "درآمد ناخالص"},
    {"Maintenance spend", "هزینهٔ تعمیرات"},
    {"NET revenue", "درآمد خالص"},
    {"Top earner", "بیشترین درآمد"},
    {" car(s)", " خودرو"},
    {"\n===== FLEET ANALYTICS | ", "\n===== تحلیل ناوگان | "},
    {"Fleet composition", "ترکیب ناوگان"},
    {"Fleet by daily-rate bucket", "ناوگان بر اساس بازهٔ قیمت "
                                   "روزانه"},
    {"Active bookings per car", "رزروهای فعال به‌ازای هر "
                                "خودرو"},
    {"Waitlist depth per car", "عمق صف انتظار هر خودرو"},
    {"Upcoming pickups", "دریافت‌های پیش‌رو"},
    {"   (no live bookings)\n", "   (رزرو فعالی نیست)\n"},
    {"   (nobody is waiting)\n", "   (کسی در صف نیست)\n"},
    {"   (nothing scheduled)\n", "   (چیزی برنامه‌ریزی نشده)\n"},
    {"today", "امروز"},
    {"tomorrow", "فردا"},
    {"in ", "تا "},
    {" days", " روز دیگر"},
    {" booking(s) past their end date — chase up the"
     " returns.\n",
     " رزرو از تاریخ پایان گذشته "
     "— پیگیری بازگشت‌ها.\n"},
    {" customer(s) on a waitlist — serving one after"
     " every return.\n",
     " مشتری در صف انتظار — بعد "
     "از هر بازگشت یکی سرویس "
     "می‌شود.\n"},
    {" live booking(s) fleet-wide.\n", " رزرو فعال در کل ناوگان.\n"},
    {"  The audit trail is empty.\n", "  ردپای حسابرسی خالی است.\n"},
    {"\n  AUDIT TRAIL — newest ", "\n  ردپای حسابرسی — "
                                  "جدیدترین "},
    {" of ", " از "},
    {" entry(ies)\n", " رکورد\n"},
    {"\n===== LOYALTY PROGRAM OVERVIEW =====\n", "\n===== نمای برنامهٔ وفاداری "
                                                 "=====\n"},
    {"\n  No customers are enrolled yet.\n", "\n  هنوز مشتری‌ای عضو نشده "
                                             "است.\n"},
    {"\n  Members per rank\n", "\n  اعضا به‌ازای هر رتبه\n"},
    {"\n  Total members      : ", "\n  جمع اعضا           : "},
    {"\n  Outstanding points : ", "\n  امتیاز نزد اعضا    : "},
    {"\n  Potential credits  : ", "\n  اعتبار قابل بازخرید: "},
    {"\n  Top member         : ", "\n  عضو برتر           : "},
    {" lifetime points)\n", " امتیاز کل)\n"},
    {"\n  Recent rank-ups\n", "\n  ارتقاهای اخیر\n"},
    {"   none yet\n", "   هنوز ندارد\n"},
    {"\n===== MONTHLY REPORT | ", "\n===== گزارش ماهانه | "},
    {"\n  No rentals were completed in ", "\n  اجاره‌ای در "},
    {".\n", " تکمیل نشد.\n"},
    {"\n  Rentals completed : ", "\n  اجاره‌های تکمیل‌شده: "},
    {"\n  Revenue booked    : ", "\n  درآمد ثبت‌شده      : "},
    {"   (discounts given: ", "   (تخفیف‌ها: "},
    {" | late fees: ", " | جریمهٔ دیرکرد: "},
    {"\n  Distinct customers: ", "\n  مشتریان متمایز     : "},
    {"Revenue by car", "درآمد به‌ازای هر خودرو"},
    {" rental(s), ", " اجاره، "},
    {"Returns per day", "بازگشت‌ها در هر روز"},
    {" return(s))\n", " بازگشت)\n"},
    {"\n  Busiest day: ", "\n  شلوغ‌ترین روز: "},
    {"  1. Add car\n  2. View fleet (insertion order)\n "
     " 3. View fleet (sorted by name - AVL index)\n  4."
     " Search fleet by keyword\n  5. Fleet statistics\n "
     " 6. Fleet analytics dashboard\n  7. Audit log "
     "(who did what)\n  8. Loyalty program overview\n  "
     "9. Income report\n 10. Export CSV & JSON reports\n"
     " 11. Block user\n 12. Unblock user\n 13. Create "
     "staff account\n 14. Save data to disk\n 15. Save &"
     " logout\n 16. Exit program\n 17. Monthly report "
     "(revenue & bookings)\n 18. Save monthly report as"
     " PNG\n",
     "  1. افزودن خودرو\n  2. مشاهدهٔ"
     " ناوگان (ترتیب افزودن)\n  3. "
     "مشاهدهٔ ناوگان (مرتب بر "
     "اساس نام - شاخص AVL)\n  4. "
     "جست‌وجوی ناوگان با "
     "کلیدواژه\n  5. آمار ناوگان\n  6."
     " داشبورد تحلیلی ناوگان\n  7. "
     "لاگ حسابرسی (چه‌کسی "
     "چه‌کاری)\n  8. نمای برنامهٔ "
     "وفاداری\n  9. گزارش درآمد\n 10. "
     "خروجی CSV و JSON\n 11. مسدودکردن "
     "کاربر\n 12. رفع مسدودی کاربر\n "
     "13. ساخت حساب کارمند\n 14. "
     "ذخیرهٔ داده‌ها روی دیسک\n "
     "15. ذخیره و خروج از حساب\n 16. "
     "خروج از برنامه\n 17. گزارش "
     "ماهانه (درآمد و رزروها)\n 18. "
     "ذخیرهٔ گزارش ماهانه "
     "به‌صورت PNG\n"},
    {"\n===== MANAGER MENU | ", "\n===== منوی مدیر | "},
    {"  Brand: ", "  برند: "},
    {"  Price per day: ", "  قیمت روزانه: "},
    {"  Car added to the fleet.\n", "  خودرو به ناوگان اضافه شد.\n"},
    {"  A car with this name already exists.\n", "  خودرویی با این نام از قبل "
                                                 "وجود دارد.\n"},
    {"  Keyword (name or brand): ", "  کلیدواژه (نام یا برند): "},
    {"  Nothing matches \"", "  چیزی هم‌خوان با «"},
    {"\n  MATCHES (", "\n  هم‌خوان‌ها ("},
    {"  Reports written:\n", "  گزارش‌ها نوشته شد:\n"},
    {"  Could not write the report files.\n", "  نوشتن فایل‌های گزارش "
                                              "ممکن نشد.\n"},
    {"  Username to block: ", "  نام کاربری برای "
                              "مسدودسازی: "},
    {"  User \"", "  کاربر «"},
    {"\" blocked.\n", "» مسدود شد.\n"},
    {"  Username to unblock: ", "  نام کاربری برای رفع "
                                "مسدودی: "},
    {"\" unblocked.\n", "» مسدودی برداشته شد.\n"},
    {"  New username: ", "  نام کاربری جدید: "},
    {"  Role (Staff / Manager): ", "  نقش (Staff / Manager): "},
    {" account created.\n", " ساخته شد.\n"},
    {"  Failed: check username is new, password >= 4 "
     "chars, role is Staff or Manager.\n",
     "  خطا: نام کاربری باید "
     "جدید، رمز حداقل 4 نویسه و "
     "نقش Staff یا Manager باشد.\n"},
    {"  Guided demo: the fleet lives in memory — "
     "nothing to save.\n",
     "  دموی راهنمادار: ناوگان در"
     " حافظه است — چیزی برای "
     "ذخیره نیست.\n"},
    {"  Data saved to ./data\n", "  داده‌ها در ./data ذخیره شد\n"},
    {"  Could not write data files.\n", "  نوشتن فایل‌های داده ممکن"
                                        " نشد.\n"},
    {"  Demo fleet kept in memory. Logged out.\n", "  ناوگان دمو در حافظه ماند. "
                                                   "از حساب خارج شدید.\n"},
    {"  Data saved. Logged out.\n", "  داده‌ها ذخیره شد. از حساب"
                                    " خارج شدید.\n"},
    {"  Demo fleet kept in memory. Goodbye!\n", "  ناوگان دمو در حافظه ماند. "
                                                "خداحافظ!\n"},
    {"  Data saved. Goodbye!\n", "  داده‌ها ذخیره شد. "
                                 "خداحافظ!\n"},
    {"  Year (e.g. 2026): ", "  سال (مثلاً 2026): "},
    {"  Month (1-12): ", "  ماه (1-12): "},
    {"  Saved to ", "  ذخیره شد در "},
    {"  Could not write ", "  نوشتن ممکن نشد برای "},
    {"  Guided demo fleet: fresh, in-memory — ./data"
     " is untouched.\n",
     "  ناوگان دموی راهنمادار: "
     "تازه و در حافظه — ./data "
     "دست‌نخورده.\n"},
    {"  Loaded saved state from ./data\n", "  وضعیت ذخیره‌شده از ./data "
                                           "بارگذاری شد\n"},
    {"  First run: demo fleet + accounts seeded.\n",
     "  اجرای اول: ناوگان و "
     "حساب‌های دمو ساخته شدند.\n"},
    {"    manager  -> admin / manager123\n    staff    "
     "-> sara / staff123\n    customer -> ali  / "
     "customer123\n",
     "    مدیر    -> admin / manager123\n    "
     "کارمند  -> sara / staff123\n    مشتری "
     "  -> ali  / customer123\n"},
    {"\n===== MAIN MENU =====\n", "\n===== منوی اصلی =====\n"},
    {"  1. Register (customer)\n  2. Login\n  3. Exit\n",
     "  1. ثبت‌نام (مشتری)\n  2. ورود\n"
     "  3. خروج\n"},
    {"  Goodbye!\n", "  خداحافظ!\n"},
    {"\n===== PICKUP REMINDERS | next ", "\n===== یادآور دریافت‌ها | "},
    {" days =====\n", " روز آینده =====\n"},
    {"  Review or cancel them in My reservations.\n", "  بازبینی یا لغو در "
                                                      "«رزروهای من».\n"},
    {"\n  ----------------- INCOME REPORT "
     "-----------------\n",
     "\n  ----------------- گزارش درآمد "
     "-----------------\n"},
    {"CAR", "خودرو"},
    {"  Gross income", "  درآمد ناخالص"},
    {"  Maintenance costs", "  هزینه‌های تعمیرات"},
    {"  NET revenue", "  درآمد خالص"},
    {"  --serve and --demo cannot be combined.\n", "  --serve و --demo با هم جمع "
                                                   "نمی‌شوند.\n"},
};

const std::unordered_map<std::string, const char *> &map()
{
    static const std::unordered_map<std::string, const char *> m = []
    {
        std::unordered_map<std::string, const char *> mm;
        for (const Pair &p : kTable)
            mm.emplace(p.en, p.fa);
        return mm;
    }();
    return m;
}

Lang g_lang = Lang::En;
const char *kDataFile = "data/lang.txt";

} // namespace

void setLang(Lang lang)
{
    g_lang = lang;
}

bool setLangByName(const std::string &name)
{
    std::string lower;
    for (char c : name)
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower == "fa")
    {
        setLang(Lang::Fa);
        return true;
    }
    if (lower == "en")
    {
        setLang(Lang::En);
        return true;
    }
    return false;
}

Lang lang()
{
    return g_lang;
}

const char *tr(const char *key)
{
    if (key == nullptr || *key == '\0' || g_lang == Lang::En)
        return key;
    const auto &m = map();
    const auto it = m.find(key);
    return it == m.end() ? key : it->second;
}

std::string trStr(const std::string &key)
{
    return tr(key.c_str());
}

void loadPersisted()
{
    std::ifstream in(kDataFile);
    std::string value;
    if (in >> value)
        setLangByName(value); // unknown tokens silently keep English
}

bool persist()
{
    std::error_code ec;
    std::filesystem::create_directories("data", ec);
    std::ofstream out(kDataFile);
    if (!out)
        return false;
    out << langCode() << "\n";
    return static_cast<bool>(out);
}

const char *langCode()
{
    return g_lang == Lang::Fa ? "fa" : "en";
}

} // namespace i18n
