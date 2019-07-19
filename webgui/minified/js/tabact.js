function refreshCurrentAction(e){L("#currentAction").innerHTML=getReadable(flip.getConfig(e))}function initAdditionalData(e){switch(L.setVisible("#WRAPPER_"+C.ADDITIONAL_FIELD_TEXT,!1),L.setVisible("#WRAPPER_"+C.ADDITIONAL_FIELD_SELECT,!1),e){case C.AT_CMD_LOAD_SLOT:L.setVisible("#WRAPPER_"+C.ADDITIONAL_FIELD_SELECT),L("#"+C.ADDITIONAL_FIELD_SELECT).innerHTML=L.createSelectItems(flip.getSlots()),L("[for="+C.ADDITIONAL_FIELD_SELECT+"]")[0].innerHTML="Slot";break;case C.AT_CMD_REST:L.setVisible("#WRAPPER_"+C.ADDITIONAL_FIELD_TEXT),L("[for="+C.ADDITIONAL_FIELD_TEXT+"]")[0].innerHTML="REST URL";break;case C.AT_CMD_MQTT_PUBLISH:L.setVisible("#WRAPPER_"+C.ADDITIONAL_FIELD_TEXT),L("[for="+C.ADDITIONAL_FIELD_TEXT+"]")[0].innerHTML="MQTT parameter"}}function resetSelects(){var e=flip.getConfig(L("#selectActionButton").value);L("#SELECT_"+C.LEARN_CAT_MOUSE).value=e,L("#SELECT_"+C.LEARN_CAT_FLIPACTIONS).value=e,L("#SELECT_"+C.LEARN_CAT_IR).value=e}function processForQueue(e){if(window.tabAction.queue=window.tabAction.queue||[],C.SUPPORTED_KEYCODES.includes(parseInt(e.keyCode))){var t=getText(tabAction.queue),n=e.keyCode==C.JS_KEYCODE_BACKSPACE&&t,i=t&&e.keyCode!=C.JS_KEYCODE_SPACE&&isSpecialKey(e),a=0<tabAction.queue.length&&tabAction.queue[tabAction.queue.length-1].key==e.key&&isSpecialKey(e);n||i||a?(n||a)&&tabAction.queue.pop():tabAction.queue.push(e),tabAction.evalRec()}}function getQueueElem(t){return{key:t.key,keyCode:t.keyCode||e.which,altKey:t.altKey}}function isPrintableKey(e){return 1===e.key.length}function isSpecialKey(e){return 1!==e.key.length}function getText(e){for(var t="",n=0;n<e.length;n++){var i=e[n],a=i.keyCode;if(isSpecialKey(i)){var o=isAltGrLetter(i,e[n+1],e[n+2]);if(a!=C.JS_KEYCODE_SHIFT&&!o)return null;o&&n++}else t+=i.key}return t}function isAltGrLetter(e,t,n){return!!(e&&t&&n)&&(e.keyCode==C.JS_KEYCODE_CTRL&&t.keyCode==C.JS_KEYCODE_ALT&&isPrintableKey(n)&&n.keyCode&&n.altKey)}function getAtCmd(e){if(!e||0==e.length)return"";var t,n=getText(e);if(n&&1<n.length)t=C.AT_CMD_WRITEWORD+" "+n;else{var i="";e.forEach(function(e){var t=C.KEYCODE_MAPPING[e.keyCode];t&&(i+=t+" ")}),t=C.AT_CMD_KEYPRESS+" "+i.trim()}return t.substring(0,C.MAX_LENGTH_ATCMD)}function getReadable(e){if(!e)return"";var t=e.substring(0,C.LENGTH_ATCMD_PREFIX-1),n=e.substring(C.LENGTH_ATCMD_PREFIX);return t==C.AT_CMD_KEYPRESS&&(n=n.replace(/ /g," + ")),n=n.replace(/KEY_/g,""),L.translate(t,n+" ")}window.tabAction={},window.tabAction.init=function(e){e=e||C.LEARN_CAT_KEYBOARD,tabAction.initBtnModeActionTable();var t=flip.getConfig(flip.FLIPMOUSE_MODE)===C.FLIPMOUSE_MODE_MOUSE?C.BTN_MODES_WITHOUT_STICK:C.BTN_MODES;L("#selectActionButton").innerHTML=L.createSelectItems(t),L("#currentAction").innerHTML=getReadable(flip.getConfig(C.BTN_MODES[0])),L("#"+flip.getConfig(flip.FLIPMOUSE_MODE)).checked=!0,L("#"+e).checked=!0,L("#SELECT_LEARN_CAT_MOUSE").innerHTML=L.createSelectItems(C.AT_CMDS_MOUSE),L("#SELECT_LEARN_CAT_FLIPACTIONS").innerHTML=L.createSelectItems(C.AT_CMDS_FLIP),L("#SELECT_LEARN_CAT_KEYBOARD_SPECIAL").innerHTML=L.createSelectItems(C.SUPPORTED_KEYCODES,function(e){return C.KEYCODE_MAPPING[e]},"SELECT_SPECIAL_KEY"),flip.sendATCmd(C.AT_CMD_IR_LIST).then(function(e){if(e){var t=e.trim().split("\n").map(function(e){return e.substring(e.indexOf(":")+1)});L("#SELECT_LEARN_CAT_IR").innerHTML=L.createSelectItems(t)}})},window.tabAction.initBtnModeActionTable=function(){L.removeAllChildren("#currentConfigTb");var r=!1,l='<span class="hidden" aria-hidden="false">'+L.translate("DESCRIPTION")+"</span>",E='<span class="hidden" aria-hidden="false">'+L.translate("CURR_ACTION")+"</span>",u='<span class="hidden" aria-hidden="false">'+L.translate("CURR_AT_CMD")+"</span>",e=flip.getConfig(flip.FLIPMOUSE_MODE)===C.FLIPMOUSE_MODE_MOUSE?C.BTN_MODES_WITHOUT_STICK:C.BTN_MODES,s=!1;if(e.forEach(function(e){var t=L.createElement("li","row",null,r?"background-color: #e0e0e0;":null),n=L.createElement("a","",L.translate(e));n.href='javascript:tabAction.selectActionButton("'+e+'")',n.title=L.translate("CHANGE_TOOLTIP",L.translate(e));var i=L.createElement("div","two columns",[l,n]),a=L.createElement("div","four columns",[E,getReadable(flip.getConfig(e))]);s=s||flip.isConfigUnsaved(e);var o=flip.isConfigUnsaved(e)?'<b style="color: red">'+flip.getConfig(e)+" *</b>":flip.getConfig(e),c=L.createElement("div","four columns",[u,o]),A=L.createElement("div","one column show-mobile space-bottom");t.appendChild(i),t.appendChild(a),t.appendChild(c),t.appendChild(A),L("#currentConfigTb").appendChild(t),r=!r}),L.setVisible("#tabActSaveBtnSpacer",!s,"initial"),s){var t=L.createElement("li","row",'<b style="color: red">'+L.translate("UNSAVED_MODE")+"</b>","margin-top: 1em");L("#currentConfigTb").appendChild(t)}},window.tabAction.selectActionButton=function(e){L("#selectActionButton").value=e,L("#selectActionButton").focus(),refreshCurrentAction(e),resetSelects(),initAdditionalData()},window.tabAction.selectActionCategory=function(e){L.setVisible("[id^=WRAPPER_LEARN_CAT]",!1),L.setVisible("#WRAPPER_"+e.id),resetSelects(),initAdditionalData()},window.tabAction.selectMode=function(e){flip.setFlipmouseMode(e.id),tabAction.init()},tabAction.selectAtCmd=function(e){initAdditionalData(tabAction.selectedAtCommand=e),C.ADDITIONAL_DATA_CMDS.includes(e)||tabAction.setAtCmd(e)},tabAction.setAtCmd=function(e){var t=L("#selectActionButton").value;e&&t&&(flip.setButtonAction(t,e),refreshCurrentAction(L("#selectActionButton").value),tabAction.initBtnModeActionTable())},tabAction.setAtCmdWithAdditionalData=function(e){tabAction.setAtCmd(tabAction.selectedAtCommand+" "+e)},tabAction.handleKeyBoardEvent=function(e){e.keyCode!=C.JS_KEYCODE_TAB&&(229!=e.keyCode?(e.preventDefault(),e.repeat||processForQueue(getQueueElem(e))):tabAction.listenToKeyboardInput=!0)},tabAction.handleOnKeyboardInput=function(e){!tabAction.listenToKeyboardInput||tabAction.lastInputLength>=e.target.value.length?tabAction.lastInputLength>e.target.value.length&&(tabAction.queue.pop(),tabAction.evalRec()):(tabAction.listenToKeyboardInput=!1,Array.from(e.data).forEach(function(e){var t=e.toUpperCase().charCodeAt(0);processForQueue({key:e,keyCode:t})}))},tabAction.addSpecialKey=function(e){window.tabAction.queue=window.tabAction.queue||[],getText(tabAction.queue)&&tabAction.resetRec(),processForQueue({key:"SPECIAL_"+e,keyCode:e}),L("#SELECT_LEARN_CAT_KEYBOARD_SPECIAL").value=-1,L("#INPUT_LEARN_CAT_KEYBOARD").focus()},tabAction.evalRec=function(){var e=getAtCmd(tabAction.queue);L("#buttonRecOK").disabled=!e,L("#recordedAtCmd").innerHTML=e||L.translate("NONE_BRACKET");var t=getReadable(e),n=L("#recordedActionA11y").innerHTML,i=L.translate("ENTERED_ACTION")+(t||L.translate("NONE_BRACKET"));n!=i&&(L("#recordedActionA11y").innerHTML=i),L("#INPUT_LEARN_CAT_KEYBOARD").value=t,tabAction.lastInputLength=t.length},tabAction.resetRec=function(){tabAction.lastInputLength=0,tabAction.queue=[],tabAction.evalRec()},tabAction.saveRec=function(){document.onkeydown&&(document.onkeydown=null,L.toggle("#start-rec-button-normal","#start-rec-button-rec")),tabAction.setAtCmd(getAtCmd(tabAction.queue))},tabAction.deleteIRCommand=function(e){e&&(flip.sendATCmdWithParam(C.AT_CMD_IR_DELETE,e),tabAction.init(C.LEARN_CAT_IR))},tabAction.recordIRCommand=function(){actionAndToggle(flip.sendATCmdWithParam,[C.AT_CMD_IR_RECORD,L("#INPUT_LEARN_CAT_IR").value,1e4],["#record-action-button-normal","#record-action-button-saving"]).then(function(){tabAction.init(C.LEARN_CAT_IR)})};