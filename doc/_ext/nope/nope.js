function show_tab(bar_node, tab_id) {
    const tabs_node = bar_node.parentNode;

    const tabs = tabs_node.querySelectorAll(".nope-tab-on, .nope-tab-off");
    const tab_name = "nope-tab-" + tab_id
    for (const tab of tabs) {
        if (tab.classList.contains(tab_name))
            tab.className = tab.className.replace("-off", "-on");
        else
            tab.className = tab.className.replace("-on", "-off");
    }

    const btns = bar_node.querySelectorAll(".nope-btn-on, .nope-btn-off");
    const btn_name = "nope-btn-" + tab_id
    for (const btn of btns) {
        if (btn.classList.contains(btn_name))
            btn.className = btn.className.replace("-off", "-on");
        else
            btn.className = btn.className.replace("-on", "-off");
    }
}
