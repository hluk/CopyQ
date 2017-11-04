// This is example plugin script for CopyQ.
// Plugin scripts should be copied to plugins directory
// (usually ~/.config/copyq/plugins/).

// Override popup()
popup = function(title, message, time)
{
    if (!time)
        time = 1000
    if (!message)
        message = ""
    execute("notify-send", "--expire-time=" + time, title, message)
}

function copyq_script() {
    popup("Script loaded!")
    return {
        name: "Test Script",
        author: function() { return "author@example.com" },
        description: function() { return "A script for testing" },
        formatsToSave: ["test1", "test2"],

        copyItem: function(data) {
            data['copy'] = "copied"
            return data
        },

        transformItemData: function(data) {
            data[mimeText] = "TEST: " + str(data[mimeText])
            return data;
        },
    }
}
