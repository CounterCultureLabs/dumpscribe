

var router = new Grapnel();

function flash(str) {
    $('#flash').html(str);
    $('#flash').css('display', 'block');
}

function hide_flash() {
    $('#flash').css('display', 'none');
}

function init() {

    router.get('', function(req){
        hide_flash();
        $('#extra').html('');
        $.getJSON('notebooks', function(val) {
            var notebooks = val.data;
            var tmpl = _.template($('#notebook-summary-template').html());

            $('#pagetitle').html("Notebooks");
            
            var html = '';
            var key, notebook;
            for(key in notebooks) {
                notebook = notebooks[key];
                html += tmpl(notebook);
            }
            if(html == '') {
                html == "<p>No notebooks detected.</p>";
            }
            $('#container').html(html);
            
            $('.notebook-summary').click(function(e) {
                var nbs = $(e.target).closest('.notebook-summary');
                var id = nbs.find('.id').val();
                window.location = '#notebook/' + id;
            });
        });
    });
    
    router.get('notebook/*', function(req) {
        hide_flash();
        $('#extra').html('');
        var order = 'date';
        var parts = req.params[0].split('/');
        var notebook_id = parts[0];
        if(parts.length > 1) {
            order = parts[1];
        }

        $.getJSON('notebook/'+notebook_id+'?order='+order, function(val) {
            var notebook = val.data;
            var pages = notebook.pages;
            var tmpl = _.template($('#page-template').html());

            var change_name_url = "#notebook-change-name/" + notebook.id;
            var change_name_text = "set name";
            if(notebook.name) {
                change_name_text = "change name";
            }
            var name = notebook.name || notebook.id;
            var header = 'Notebook: '+name+'<a class="change-name" href="'+change_name_url+'">'+change_name_text+'</a>';
            $('#pagetitle').html(header);

            var menu = '<p><a href="'+encodeURI(notebook.pdf)+'">Download complete notebook PDF</a></p>';
            if(order == 'date') {
                menu += '<p class="order">Order by last updated | <a href="#notebook/'+notebook.id+'/pagenumber">Order by page number</a>';
            } else {
                menu += '<p class="order"><a href="#notebook/'+notebook.id+'/date">Order by last updated</a> | Order by page number';
            }

            $('#extra').html(menu);

            var html = '';
            var i;
            for(i=0; i < pages.length; i++) {
                html += tmpl(pages[i]);
            }

            if(html == '') {
                html = "<p>This notebook has no pages.</p>";
            }

            $('#container').html(html);
        });
        
    });


    router.get('notebook-change-name/*', function(req) {
        hide_flash();
        $('#extra').html('');
        var notebook_id = req.params[0];
        if(!notebook_id) {
            $('#pagetitle').html("Error");
            $('#container').html("<p>Cannot change notebook name without specifying notebook ID.</p>");
            return;
        }            

        $('#pagetitle').html("Change notebook name");

        var tmpl = _.template($('#change-name-template').html());

        html = tmpl({
            notebook_id: notebook_id
        });

        $('#container').html(html);
        $('form').submit(function(e) {
            e.preventDefault();
            hide_flash();

            var id = $("input[name='id']").val();
            var q = {
                name: $("input[name='name']").val(),
                password: $("input[name='password']").val()
            };

            if(!id || !q.name || !q.password) {
                flash("You must fill out both name and password. Hint: The password is set in the settings.js file on the server.");
                return false;
            }

            $.post('notebook-change-name/'+id, q, function(response) {
                flash("Name successfully changed! (redirecting...)");
                setTimeout(function() {
                    window.location.href = '#notebook/'+notebook_id;
                }, 3000);
            }, 'json').fail(function(xhr) {
                console.log(xhr.responseText);
                var resp = JSON.parse(xhr.responseText);
                flash("Error: " + resp.msg);
            });
            
            return false;
        });
        
    });
}


$(document).ready(init);
