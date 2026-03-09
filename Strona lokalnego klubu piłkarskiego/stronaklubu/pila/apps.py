from django.apps import AppConfig


class PilaConfig(AppConfig):
    default_auto_field = 'django.db.models.BigAutoField'
    name = 'pila'
    
    def ready(self):
        import pila.models  