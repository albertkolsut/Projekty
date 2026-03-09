from django.contrib import admin
from .models import Post, Comment
from .models import Match, Ticket, TicketType
from .models import ZawodnikStatystyka



@admin.register(ZawodnikStatystyka)
class ZawodnikStatystykaAdmin(admin.ModelAdmin):
    list_display = ('imie_nazwisko', 'numer', 'wystepy', 'gole', 'asysty', 'zolt_kartki', 'czer_kartki')
    search_fields = ('imie_nazwisko',)
class PostAdmin(admin.ModelAdmin):
    list_display = ('title', 'author', 'created_date', 'published_date')
    prepopulated_fields = {'slug': ('title',)}
    
class CommentAdmin(admin.ModelAdmin):
    list_display = ('author', 'post', 'created_date', 'approved')
    list_filter = ('approved', 'created_date')
    search_fields = ('author', 'text')
    actions = ['approve_comments']

    def approve_comments(self, request, queryset):
        queryset.update(approved=True)

@admin.register(Match)
class MatchAdmin(admin.ModelAdmin):
    list_display = ('opponent_name', 'match_date', 'location', 'base_ticket_price')
    list_filter = ('match_date', 'location')
    search_fields = ('opponent_name',)

@admin.register(TicketType)
class TicketTypeAdmin(admin.ModelAdmin):
    list_display = ('name', 'discount_percentage')

@admin.register(Ticket)
class TicketAdmin(admin.ModelAdmin):
    list_display = ('user', 'match', 'quantity', 'purchase_date')
    list_filter = ('purchase_date', 'match__match_date', 'user')
    search_fields = ('user__username', 'match__opponent_name')
    # readonly_fields = ()

admin.site.register(Post, PostAdmin)
admin.site.register(Comment, CommentAdmin)




