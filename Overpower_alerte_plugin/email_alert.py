# === email_alert.py ==========================================================
# Script appelé par le plugin C++ :
#   python3 email_alert.py "<subject>" "<body>" "<destinataire>"
#
# Il utilise l'API Gmail (OAuth2) avec 'credentials.json' (dans le même dossier)
# pour envoyer l'email. Au premier lancement, une fenêtre de navigateur
# demandera l’autorisation ; un 'token.pickle' sera créé, puis réutilisé.

import sys                      # Lire les arguments de la ligne de commande
import os                       # Gérer les chemins de fichiers
import pickle                   # Sauvegarder/charger le token OAuth
from email.mime.text import MIMEText   # Construire un courriel texte
from email.mime.multipart import MIMEMultipart
import base64                   # Encoder le message pour l'API Gmail

# Librairies Google pour OAuth2 + Gmail API
from google_auth_oauthlib.flow import InstalledAppFlow
from google.auth.transport.requests import Request
from googleapiclient.discovery import build

# Portée minimale pour envoyer un e-mail
SCOPES = ["https://www.googleapis.com/auth/gmail.send"]

from google.auth.exceptions import RefreshError

def get_gmail_service():
    
    BASE_DIR  = os.path.dirname(os.path.abspath(__file__))
    CRED_PATH = os.path.join(BASE_DIR, "credentials.json")
    TOKEN_PATH = os.path.join(BASE_DIR, "token.pickle")

    creds = None
    if os.path.exists(TOKEN_PATH):
        with open(TOKEN_PATH, "rb") as f:
            creds = pickle.load(f)

    def do_auth():
        flow = InstalledAppFlow.from_client_secrets_file(CRED_PATH, SCOPES)
        if "DISPLAY" in os.environ:
            return flow.run_local_server(port=0)
        else:
            return flow.run_console()

    if not creds:
        creds = do_auth()
        with open(TOKEN_PATH, "wb") as f:
            pickle.dump(creds, f)
    else:
        try:
            if creds.expired and creds.refresh_token:
                creds.refresh(Request())
            elif not creds.valid:
                creds = do_auth()
                with open(TOKEN_PATH, "wb") as f:
                    pickle.dump(creds, f)
        except RefreshError:
            # token révoqué/périmé → on repart sur une auth propre
            try: os.remove(TOKEN_PATH)
            except Exception: pass
            creds = do_auth()
            with open(TOKEN_PATH, "wb") as f:
                pickle.dump(creds, f)

    return build("gmail", "v1", credentials=creds)

def send_email(subject: str, body: str, to_email: str):
    """
    Construit un e-mail (texte) et l’envoie via l’API Gmail.
    """
    # Récupère le service authentifié
    service = get_gmail_service()

    # Construit le message MIME (from=“me” → le compte connecté)
    msg = MIMEMultipart()
    msg["to"] = to_email
    msg["subject"] = subject
    msg.attach(MIMEText(body, "plain"))

    # Encode le message au format attendu par Gmail API
    raw = base64.urlsafe_b64encode(msg.as_bytes()).decode("utf-8")
    body_req = {"raw": raw}

    # Envoi via Gmail API
    service.users().messages().send(userId="me", body=body_req).execute()

if __name__ == "__main__":
    # Attend 3 arguments : sujet, corps, destinataire
    if len(sys.argv) != 4:
        print("Usage: email_alert.py \"<subject>\" \"<body>\" \"<to_email>\"")
        sys.exit(1)

    subject = sys.argv[1]  # Sujet passé par le plugin C++
    body    = sys.argv[2]  # Corps du message (multi-lignes possible)
    to      = sys.argv[3]  # Adresse destinataire

    # Envoi du message
    send_email(subject, body, to)
