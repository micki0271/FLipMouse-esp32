window.tabSlot={},tabSlot.initSlots=function(){var e=flip.getSlots();L("#delete-slot-button").disabled=e.length<=1,L("#create-slot-button").disabled=!0,L(".slot-select").forEach(function(t){t.innerHTML=L.createSelectItems(e),t.value=flip.getCurrentSlot()})},window.tabSlot.selectSlot=function(e){var t=flip.setSlot(e.value);L(".slot-select").forEach(function(t){t.value=e.value}),tabAction.init(),initWithConfig(t)},window.tabSlot.saveSlotLabelChanged=function(t){L("#create-slot-button").disabled=!t.value},window.tabSlot.createSlot=function(t,e){var l=L("#newSlotLabelEn")?L("#newSlotLabelEn").value:L("#newSlotLabelDe").value;actionAndToggle(flip.createSlot,[l],t,e).then(function(){tabSlot.initSlots(),L.setValue("#newSlotLabelEn",""),L.setValue("#newSlotLabelDe","")})},window.tabSlot.deleteSlot=function(t,e){var l=L("#selectSlotDelete").value,o=L.translate("CONFIRM_DELETE_SLOT",l);window.confirm(o)&&actionAndToggle(flip.deleteSlot,[l],t,e).then(function(){tabSlot.initSlots(),L.setValue("#newSlotLabelEn",""),L.setValue("#newSlotLabelDe","")})},window.tabSlot.resetConfig=function(t,e){var l=L.translate("CONFIRM_RESET_SLOTS");window.confirm(l)&&actionAndToggle(flip.restoreDefaultConfiguration,[],t,e)};