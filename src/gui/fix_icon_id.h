/*
    Copyright (c) 2020, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FIX_ICON_ID_H
#define FIX_ICON_ID_H

#include "gui/icons.h"

/// Backwards compatibility with old icon font.
/// Returns icon ID in the new icon font for an ID from the old one.
inline unsigned short fixIconId(unsigned short id)
{
    switch (id) {

    // envelope-o, notification, support, e-mail, letter, mail, email
    case 0xf003: return IconEnvelope; // notification, support, envelope, e-mail, letter, mail, message, email

    // rating, favorite, star-o, score, award, night, achievement
    case 0xf006: return IconStar; // rating, star, favorite, award, score, night, achievement

    // hide, garbage, trash-o, remove, trash, delete
    case 0xf014: return IconTrash; // trash, hide, garbage, remove, delete

    // file-o, new, document, page, pdf
    case 0xf016: return IconFile; // new, document, page, file, pdf

    // download, arrow-circle-o-down
    case 0xf01a: return IconDownload; // download, import

    // arrow-circle-o-up
    case 0xf01b: return IconArrowAltCircleUp; // arrow-alt-circle-up, arrow-circle-o-up

    // play-circle-o
    case 0xf01d: return IconPlayCircle;

    // write, pencil, edit, update
    case 0xf040: return IconPencilAlt; // write, edit, pencil-alt, update, pencil

    // send, share-square-o, arrow, social
    case 0xf045: return IconExternalLinkAlt;

    // ok, confirm, check-square-o, done, accept, todo, agree
    case 0xf046: return IconCheckSquare; // ok, confirm, checkmark, accept, done, check-square, todo, agree

    // move, arrows, reorder, resize
    case 0xf047: return IconArrowsAlt; // fullscreen, move, arrows, enlarge, arrows-alt, resize, arrow, expand, bigger, reorder

    // close, times-circle-o, exit, x
    case 0xf05c: return IconTimesCircle; // close, times-circle, exit, x

    // ok, confirm, accept, done, todo, agree, check-circle-o
    case 0xf05d: return IconCheckCircle; // check-circle, ok, confirm, accept, done, todo, agree

    // resize, arrows-v
    case 0xf07d: return IconArrowsAltV; // arrows-alt-v, resize, arrows-v

    // arrows-h, resize
    case 0xf07e: return IconArrowsAltH; // arrows-h, arrows-alt-h, resize

    // thumbs-o-up, like, favorite, hand, approve, agree
    case 0xf087: return IconThumbsUp; // thumbs-o-up, thumbs-up, like, favorite, hand, approve, agree

    // disapprove, dislike, disagree, thumbs-o-down, hand
    case 0xf088: return IconThumbsDown; // thumbs-down, thumbs-o-down, disagree, disapprove, hand, dislike

    // love, heart-o, favorite, like
    case 0xf08a: return IconHeart; // heart, favorite, like, love

    // sign-out, log out, leave, logout, arrow, exit
    case 0xf08b: return IconSignOutAlt; // sign-out, log out, leave, logout, arrow, exit, sign-out-alt

    // new, open, external-link
    case 0xf08e: return IconFileAlt; // file-alt, file-text, new, pdf, document, page

    // join, signup, enter, sign-in, signin, sign up, arrow, sign in, login, log in
    case 0xf090: return IconSignInAlt; // sign-in-alt, join, signup, enter, sign-in, signin, sign up, arrow, sign in, login, log in

    // box, square-o, square, block
    case 0xf096: return IconSquare; // box, square, block

    // bookmark-o, save
    case 0xf097: return IconBookmark; // bookmark, save

    // notification, reminder, bell-o, alert
    case 0xf0a2: return IconBell; // notification, reminder, bell, alert

    // purchase, buy, money, cash, checkout, payment
    case 0xf0d6: return IconMoneyBillAlt; // money, money-bill-alt

    // tachometer
    case 0xf0e4: return IconTachometerAlt; // tachometer, tachometer-alt

    // feedback, notification, sms, comment-o, note, conversation, speech, chat, texting, message, bubble
    case 0xf0e5: return IconComment; // comment, feedback, notification, sms, note, conversation, speech, chat, texting, message, bubble

    // feedback, notification, sms, note, conversation, speech, chat, texting, comments-o, message, bubble
    case 0xf0e6: return IconComments; // feedback, notification, sms, comments, note, conversation, speech, chat, texting, message, bubble

    // transfer, arrows, arrow, exchange
    case 0xf0ec: return IconExchangeAlt; // transfer, arrows, exchange-alt, arrow, exchange

    // import, cloud-download
    case 0xf0ed: return IconCloudDownloadAlt; // cloud-download-alt, cloud-download

    // import, cloud-upload
    case 0xf0ee: return IconCloudUploadAlt; // cloud-upload-alt, cloud-upload

    // restaurant, food, spoon, cutlery, dinner, eat, knife
    case 0xf0f5: return IconUtensils; // restaurant, food, spoon, cutlery, utensils, dinner, eat, knife

    // file-text-o, document, page, new, pdf
    case 0xf0f6: return IconFileAlt; // file-alt, file-text, new, pdf, document, page

    // apartment, business, office, company, work, building-o
    case 0xf0f7: return IconBuilding; // building, apartment, business, office, company, work

    // circle-o
    case 0xf10c: return IconCircle;

    // reply
    case 0xf112: return IconReply; // reply

    // folder-o
    case 0xf114: return IconFolder;

    // folder-open-o
    case 0xf115: return IconFolderOpen;

    // report, notification, flag-o
    case 0xf11d: return IconFlag; // report, notification, flag, notify

    // star-half-o, score, award, achievement, rating
    case 0xf123: return IconStarHalf; // rating, star-half-empty, star-half-full, award, score, star-half, achievement

    // security, winner, award, shield, achievement
    case 0xf132: return IconShieldAlt; // shield, shield-alt

    // movie, ticket, support, pass
    case 0xf145: return IconTicketAlt; // ticket-alt, ticket

    // hide, collapse, remove, minus-square-o, minify, trash, delete
    case 0xf147: return IconMinusSquare; // hide, collapse, remove, minus-square, minify, trash, delete

    // level-up, arrow
    case 0xf148: return IconLevelUpAlt; // level-up, level-up-alt

    // level-down, arrow
    case 0xf149: return IconLevelDownAlt; // level-down, level-down-alt

    // new, external-link-square, open
    case 0xf14c: return IconExternalLinkSquareAlt; // external-link-square-alt, external-link-square, open, new

    // video, film, youtube-square
    case 0xf166: return 0xf431;

    // start, playing, youtube-play
    case 0xf16a: return IconYoutube;

    // long-arrow-down
    case 0xf175: return IconLongArrowAltDown; // long-arrow-down, long-arrow-alt-down

    // long-arrow-up
    case 0xf176: return IconLongArrowAltUp; // long-arrow-alt-up, long-arrow-up

    // long-arrow-left, back, previous
    case 0xf177: return IconLongArrowAltLeft; // long-arrow-alt-left, previous, back, long-arrow-left

    // long-arrow-right
    case 0xf178: return IconLongArrowAltRight; // long-arrow-alt-right, long-arrow-right

    // forward, arrow-circle-o-right, next
    case 0xf18e: return IconArrowAltCircleRight; // forward, next, arrow-circle-o-right, arrow-alt-circle-right

    // arrow-circle-o-left, back, previous
    case 0xf190: return IconArrowAltCircleLeft; // arrow-circle-o-left, previous, back, arrow-alt-circle-left

    // new, add, create, expand, plus-square-o
    case 0xf196: return IconPlusSquare; // new, add, plus-square, create, expand

    // spoon
    case 0xf1b1: return IconUtensilSpoon; // spoon, utensil-spoon

    // circle-thin
    case 0xf1db: return IconCircle; // notification, circle, dot, circle-thin

    // bell-slash-o
    case 0xf1f7: return IconBellSlash;

    // diamond, gemstone, gem
    case 0xf219: return IconGem; // diamond, gem

    // sticky-note-o
    case 0xf24a: return IconStickyNote;

    // hourglass-o
    case 0xf250: return IconHourglass;

    // map-o
    case 0xf278: return IconMap;

    // commenting-o, feedback, sms, texting, note, conversation, speech, chat, notification, message, bubble
    case 0xf27b: return IconCommentAlt; // commenting, feedback, notification, sms, note, conversation, speech, chat, texting, comment-alt, bubble, message

    // purchase, buy, money, debit, credit card, checkout, payment, credit-card-alt
    case 0xf283: return IconCreditCard; // purchase, buy, money, credit-card, debit, checkout, payment, credit-card-alt

    // pause-circle-o
    case 0xf28c: return IconPauseCircle;

    // stop-circle-o
    case 0xf28e: return IconStopCircle;

    // wheelchair-alt, handicap, person
    case 0xf29b: return IconWheelchair; // handicap, person, wheelchair

    // question-circle-o
    case 0xf29c: return IconQuestionCircle;

    // envelope-open-o
    case 0xf2b7: return IconEnvelopeOpen;

    // address-book-o
    case 0xf2ba: return IconAddressBook;

    // address-card-o
    case 0xf2bc: return IconAddressCard;

    // user-circle-o
    case 0xf2be: return IconUserCircle;

    // user-o
    case 0xf2c0: return IconUser;

    // id-card-o
    case 0xf2c3: return IconIdCard;

    // window-close
    case 0xf2d3: return IconWindowClose; // window-close

    // window-close-o
    case 0xf2d4: return IconWindowClose;

    default:
        return id;
    }
}

#endif // FIX_ICON_ID_H
