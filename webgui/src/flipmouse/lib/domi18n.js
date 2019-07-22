/*! https://github.com/ruyadorno/dom-i18n */
!function(a,b){"use strict";"function"==typeof define&&define.amd?define([],function(){return a.domI18n=b()}):"object"==typeof exports?module.exports=b():a.domI18n=b()}(this,function(){"use strict";return function(a){function b(a){return a||(a=window.navigator.languages?window.navigator.languages[0]:window.navigator.language||window.navigator.userLanguage),-1===p.indexOf(a)&&(console.warn(a+" is not available on the list of languages provided"),a=a.indexOf("-")?a.split("-")[0]:a),-1===p.indexOf(a)&&(console.error(a+" is not compatible with any language provided"),a=o),a}function c(a){s=b(a),k()}function d(a){var b=a.getAttribute("data-dom-i18n-id");return b&&r&&r[b]}function e(a,b){var c="i18n"+Date.now()+1e3*Math.random();a.setAttribute("data-dom-i18n-id",c),r[c]=b}function f(a){return r&&r[a.getAttribute("data-dom-i18n-id")]}function g(a,b){var c={},d=a.firstElementChild,e=!d&&a[b].split(n);return p.forEach(function(b,f){var g;d?(g=a.children[f],g&&g.cloneNode&&(c[b]=g.cloneNode(!0))):(g=e[f],g&&(c[b]=String(g)))}),c}function h(a){var b,c,h=a.getAttribute(q),j=h?h:"textContent";d(a)?b=f(a):(b=g(a,j),e(a,b)),c=b[s],"string"==typeof c?a[j]=c:"object"==typeof c&&i(a,c)}function i(a,b){j(a),a.appendChild(b)}function j(a){for(;a.lastChild;)a.removeChild(a.lastChild)}function k(){for(var a="string"==typeof m||m instanceof String?l.querySelectorAll(m):m,b=0;b<a.length;++b)h(a[b])}a=a||{};var l=a.rootElement||window.document,m=a.selector||"[data-translatable]",n=a.separator||" // ",o=a.defaultLanguage||"en",p=a.languages||["en"],q="data-translatable-attr",r={},s=b(a.currentLanguage);return k(m),{changeLanguage:c}}});