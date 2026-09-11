import { Injectable, inject } from '@angular/core';
import { MatSnackBar } from '@angular/material/snack-bar';

/**
 * The single place that displays transient, action-triggered errors (e.g. a
 * failed Convert/GetHistory submission) as a Material snackbar -- persistent
 * "can't render this section at all" failures (e.g. the initial currency
 * list load) stay as inline state instead, since a toast that auto-dismisses
 * isn't the right affordance for a condition that blocks the whole view.
 */
@Injectable({ providedIn: 'root' })
export class NotificationService {
  private readonly snackBar = inject(MatSnackBar);

  showError(message: string): void {
    this.snackBar.open(message, 'Dismiss', { duration: 6000, panelClass: 'notification-error' });
  }
}
