// The schema. The value is provided by the build script.
var schema = $${SCHEMA};

// The default configuration.
var default_config = $${DEFAULT};

// Divs/textareas on the page
var $editor = document.getElementById('editor');

// Buttons
var $save_data_button = document.getElementById('save_data');
var $reload_data_button = document.getElementById('reload_data');
var $load_defaults_button = document.getElementById('load_defaults');
var $export_data_button = document.getElementById('export_data');
var $import_data_button = document.getElementById('import_data');
var $import_data_file_input = document.getElementById('import_data_file');

var jsoneditor;

// Helper functions for resolving references within the editor.
function aprinter_resolve_ref(target_arr, target_id_field, target_id) {
    for (var index in target_arr) {
        var target = target_arr[index];
        if (target[target_id_field] == target_id) {
            return target;
        }
    }
    return null;
}

// Function to get config from local storage or default.
function get_config(allow_default) {
    var config_json = localStorage.getItem("aprinter_config");
    if (!config_json) {
        if (allow_default) {
            return {ok: true, value: default_config};
        }
        return {ok: false};
    }
    var config_value = JSON.parse(config_json);
    return {ok: true, value: config_value};
}

var load = function() {
    // Get initial configuration from local storage, if any.
    var startval = get_config(true).value;

    // Create the editor.
    jsoneditor = new JSONEditor($editor, {
        schema: schema,
        startval: startval,
        disable_edit_json: true,
        disable_properties: true,
        template: "javascript",
        theme: "bootstrap3",
        iconlib: "bootstrap3"
    });
    
    // When the save button is pressed, save the config data to local storage.
    $save_data_button.addEventListener('click', function() {
        var config_json = JSON.stringify(jsoneditor.getValue());
        localStorage.setItem("aprinter_config", config_json);
    });
    
    // When the reload button is pressed, load the config data from local storage.
    $reload_data_button.addEventListener('click', function() {
        var get_config_res = get_config(false);
        if (!get_config_res.ok) {
            alert("Cannot load - no configuration present in local storage!");
            return;
        }
        jsoneditor.setValue(get_config_res.value);
    });
    
    // When the load-defaults button is pressed, load the default config.
    $load_defaults_button.addEventListener('click', function() {
        jsoneditor.setValue(default_config);
    });
    
    // If the window is about to be closed with unsaved data, ask for confirmation.
    window.onbeforeunload = function (evt) {
        if (JSON.stringify(get_config(true).value) == JSON.stringify(jsoneditor.getValue())) {
            return null;
        }
        var message = 'APrinter Configuration: There are unsaved configuration changes!';
        if (typeof evt == 'undefined') {
            evt = window.event;
        }
        if (evt) {
            evt.returnValue = message;
        }
        return message;
    }
    
    // When the export button is pressed, trigger downloading of the configuration dump.
    $export_data_button.addEventListener('click', function() {
        var config_json = JSON.stringify(jsoneditor.getValue());
        var blob = new Blob([config_json], {type: 'application/json;charset=utf-8'});
        saveAs(blob, 'aprinter_config.json')
    });
    
    $import_data_button.addEventListener('click', function() {
        if (!$import_data_file_input.files[0]) {
            alert('Please select a configuration file to import.');
            return;
        }
        var reader = new FileReader();
        reader.onload = function() {
            var config = JSON.parse(this.result);
            jsoneditor.setValue(config);
        }
        reader.readAsText($import_data_file_input.files[0]);
    });
};

load();
